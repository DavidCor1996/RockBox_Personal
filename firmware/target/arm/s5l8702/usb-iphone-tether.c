/***************************************************************************
 * Minimal S5L8702 DesignWare host transport for Apple ipheth Ethernet.
 *
 * This is deliberately not a general USB host stack. It enumerates one
 * directly connected, externally powered Apple device and accepts only the
 * vendor Ethernet interface used by the upstream Linux ipheth driver.
 ****************************************************************************/
#include "config.h"
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include "kernel.h"
#include "logf.h"
#include "strlcpy.h"
#include "system.h"
#include "usb_ch9.h"
#include "usb-designware.h"
#include "usb_iphone_tether.h"

#define APPLE_VENDOR_ID             0x05ac
#define IPHETH_CLASS                0xff
#define IPHETH_SUBCLASS             0xfd
#define IPHETH_PROTOCOL             0x01
#define IPHETH_GET_MAC              0x00
#define IPHETH_CARRIER_CHECK        0x45
#define IPHETH_CARRIER_ON           0x04
#define USBMUX_CLASS                0xff
#define USBMUX_SUBCLASS             0xfe
#define USBMUX_PROTOCOL             0x02

#define HOST_FORCE_MODE             (1u << 29)
#define HPRT_CONNECT                (1u << 0)
#define HPRT_ENABLE                 (1u << 2)
#define HPRT_RESET                  (1u << 8)
#define HPRT_POWER                  (1u << 12)
#define HPRT_SPEED_MASK             (3u << 17)
#define HPRT_SPEED_LOW              (2u << 17)
#define HCCHAR_DIR_IN               (1u << 15)
#define HCCHAR_LOW_SPEED            (1u << 17)
#define HCCHAR_TYPE_CONTROL         (0u << 18)
#define HCCHAR_TYPE_BULK            (2u << 18)
#define HCCHAR_DEVADDR(x)           ((uint32_t)(x) << 22)
#define HCCHAR_ENABLE               (1u << 31)
#define HCTSIZ_PKTCNT(x)            ((uint32_t)(x) << 19)
#define HCTSIZ_PID(x)               ((uint32_t)(x) << 29)
#define HOST_PID_DATA0              0
#define HOST_PID_DATA1              2
#define HOST_PID_SETUP              3
#define HCINT_XFRC                  (1u << 0)
#define HCINT_HALTED                (1u << 1)
#define HCINT_STALL                 (1u << 3)
#define HCINT_NAK                   (1u << 4)
#define HCINT_ERRORS                ((1u << 2) | (1u << 7) | (1u << 8) | \
                                     (1u << 9) | (1u << 10))
#define HOST_XFER_TIMEOUT           (HZ / 2)
#define HOST_FRAME_MAX              1600
#define HOST_CONFIG_MAX             1024

enum host_xfer_result
{
    HOST_XFER_OK = 0,
    HOST_XFER_NAK = -1,
    HOST_XFER_STALL = -2,
    HOST_XFER_ERROR = -3,
    HOST_XFER_TIMEOUT_ERROR = -4
};

struct iphone_host
{
    enum usb_iphone_tether_state state;
    const char *status;
    uint8_t address;
    uint8_t configuration;
    uint8_t interface_number;
    uint8_t alternate_setting;
    uint8_t bulk_in;
    uint8_t bulk_out;
    uint16_t bulk_in_mps;
    uint16_t bulk_out_mps;
    uint8_t in_toggle;
    uint8_t out_toggle;
    uint8_t mux_interface_number;
    uint8_t mux_alternate_setting;
    uint8_t mux_bulk_in;
    uint8_t mux_bulk_out;
    uint16_t mux_bulk_in_mps;
    uint16_t mux_bulk_out_mps;
    uint8_t mux_in_toggle;
    uint8_t mux_out_toggle;
    bool low_speed;
    bool paired_traffic_seen;
    bool mac_valid;
    bool frame_pending;
    int frame_length;
    long next_poll;
    long connect_deadline;
    char udid[42];
};

static struct iphone_host host;
static unsigned char control_buffer[HOST_CONFIG_MAX]
    CACHEALIGN_ATTR;
static unsigned char transfer_buffer[HOST_FRAME_MAX + 2]
    CACHEALIGN_ATTR;
static unsigned char received_frame[HOST_FRAME_MAX]
    CACHEALIGN_ATTR;
static unsigned char host_mac[6];

extern void usb_dw_target_enable_clocks(void);
extern void usb_dw_target_disable_clocks(void);
extern void usb_dw_target_disable_irq(void);

static uint16_t get_le16(const void *pointer)
{
    const unsigned char *p = pointer;
    return p[0] | ((uint16_t)p[1] << 8);
}

static void set_error(const char *status)
{
    host.state = USB_IPHONE_ERROR;
    host.status = status;
    logf("ipheth host: %s", status);
}

static bool wait_register(volatile uint32_t *reg, uint32_t mask,
                          uint32_t value, long timeout)
{
    long deadline = current_tick + timeout;
    while ((*reg & mask) != value)
    {
        if (!TIME_BEFORE(current_tick, deadline))
            return false;
        yield();
    }
    return true;
}

static void channel_halt(int channel)
{
    uint32_t value = DWC_HCCHAR(channel);
    DWC_HCCHAR(channel) = value | (1u << 30) | HCCHAR_ENABLE;
    wait_register(&DWC_HCINT(channel), HCINT_HALTED, HCINT_HALTED,
                  HZ / 20 + 1);
    DWC_HCINT(channel) = HCINT_HALTED;
}

static int channel_transfer(int channel, uint8_t endpoint, bool direction_in,
                            uint32_t type, uint8_t pid, void *buffer,
                            int length, int max_packet)
{
    uint32_t interrupt;
    uint32_t packets = length ? (length + max_packet - 1) / max_packet : 1;
    uint32_t character = max_packet | ((endpoint & 0xf) << 11) | type |
                         HCCHAR_DEVADDR(host.address) | HCCHAR_ENABLE;
    long deadline = current_tick + HOST_XFER_TIMEOUT;

    if (direction_in)
    {
        character |= HCCHAR_DIR_IN;
        commit_discard_dcache_range(buffer, MAX(length, 1));
    }
    else
        commit_dcache_range(buffer, MAX(length, 1));
    if (host.low_speed)
        character |= HCCHAR_LOW_SPEED;

    DWC_HCINTMSK(channel) = 0;
    DWC_HCINT(channel) = 0xffffffff;
    DWC_HCDMA(channel) = (uintptr_t)S5L8702_PHYSICAL_ADDR(buffer);
    DWC_HCTSIZ(channel) = (length & 0x7ffff) | HCTSIZ_PKTCNT(packets) |
                          HCTSIZ_PID(pid);
    DWC_HCCHAR(channel) = character;

    do
    {
        interrupt = DWC_HCINT(channel);
        if (interrupt & HCINT_XFRC)
        {
            int actual = length - (DWC_HCTSIZ(channel) & 0x7ffff);
            DWC_HCINT(channel) = interrupt;
            if (direction_in && actual > 0)
                discard_dcache_range(buffer, actual);
            return actual;
        }
        if (interrupt & HCINT_NAK)
        {
            channel_halt(channel);
            DWC_HCINT(channel) = interrupt;
            return HOST_XFER_NAK;
        }
        if (interrupt & HCINT_STALL)
        {
            channel_halt(channel);
            DWC_HCINT(channel) = interrupt;
            return HOST_XFER_STALL;
        }
        if (interrupt & HCINT_ERRORS)
        {
            channel_halt(channel);
            DWC_HCINT(channel) = interrupt;
            return HOST_XFER_ERROR;
        }
        yield();
    }
    while (TIME_BEFORE(current_tick, deadline));

    channel_halt(channel);
    return HOST_XFER_TIMEOUT_ERROR;
}

static int control_transfer(uint8_t request_type, uint8_t request,
                            uint16_t value, uint16_t index, void *data,
                            uint16_t length)
{
    struct usb_ctrlrequest setup;
    bool data_in = (request_type & USB_DIR_IN) != 0;
    int result;

    setup.bRequestType = request_type;
    setup.bRequest = request;
    setup.wValue = value;
    setup.wIndex = index;
    setup.wLength = length;
    memcpy(control_buffer, &setup, sizeof(setup));
    result = channel_transfer(0, 0, false, HCCHAR_TYPE_CONTROL,
                              HOST_PID_SETUP, control_buffer,
                              sizeof(setup), 64);
    if (result != (int)sizeof(setup))
        return result < 0 ? result : HOST_XFER_ERROR;

    if (length)
    {
        result = channel_transfer(0, 0, data_in, HCCHAR_TYPE_CONTROL,
                                  HOST_PID_DATA1, data, length, 64);
        if (result < 0)
            return result;
    }
    else
        result = 0;

    if (channel_transfer(0, 0, !data_in, HCCHAR_TYPE_CONTROL,
                         HOST_PID_DATA1, control_buffer, 0, 64) < 0)
        return HOST_XFER_ERROR;
    return result;
}

static bool port_reset(void)
{
    uint32_t port = DWC_HPRT;
    DWC_HPRT = (port & HPRT_SPEED_MASK) | HPRT_POWER | HPRT_RESET;
    sleep(HZ / 20);
    port = DWC_HPRT;
    DWC_HPRT = (port & HPRT_SPEED_MASK) | HPRT_POWER;
    sleep(HZ / 50);
    port = DWC_HPRT;
    host.low_speed = (port & HPRT_SPEED_MASK) == HPRT_SPEED_LOW;
    return (port & (HPRT_CONNECT | HPRT_ENABLE)) ==
           (HPRT_CONNECT | HPRT_ENABLE);
}

static bool parse_tether_interface(const unsigned char *data, int length)
{
    int offset = 0;
    enum { MATCH_NONE, MATCH_IPHETH, MATCH_USBMUX } matching = MATCH_NONE;
    uint8_t in = 0, out = 0, intf = 0, alt = 0;
    uint16_t in_mps = 0, out_mps = 0;
    bool found_ipheth = false;

#define SAVE_MATCHED_INTERFACE() do {                                      \
        if (in && out && in_mps && out_mps) {                              \
            if (matching == MATCH_IPHETH) {                                \
                host.interface_number = intf;                              \
                host.alternate_setting = alt;                              \
                host.bulk_in = in;                                         \
                host.bulk_out = out;                                       \
                host.bulk_in_mps = in_mps;                                 \
                host.bulk_out_mps = out_mps;                               \
                found_ipheth = true;                                       \
            } else if (matching == MATCH_USBMUX) {                         \
                host.mux_interface_number = intf;                          \
                host.mux_alternate_setting = alt;                          \
                host.mux_bulk_in = in;                                     \
                host.mux_bulk_out = out;                                   \
                host.mux_bulk_in_mps = in_mps;                             \
                host.mux_bulk_out_mps = out_mps;                           \
            }                                                              \
        }                                                                  \
    } while (0)

    while (offset + 2 <= length && data[offset] >= 2 &&
           offset + data[offset] <= length)
    {
        int descriptor_length = data[offset];
        int type = data[offset + 1];
        if (type == USB_DT_INTERFACE && descriptor_length >= 9)
        {
            SAVE_MATCHED_INTERFACE();
            if (data[offset + 5] == IPHETH_CLASS &&
                data[offset + 6] == IPHETH_SUBCLASS &&
                data[offset + 7] == IPHETH_PROTOCOL)
                matching = MATCH_IPHETH;
            else if (data[offset + 5] == USBMUX_CLASS &&
                     data[offset + 6] == USBMUX_SUBCLASS &&
                     data[offset + 7] == USBMUX_PROTOCOL)
                matching = MATCH_USBMUX;
            else
                matching = MATCH_NONE;
            in = out = 0;
            in_mps = out_mps = 0;
            if (matching != MATCH_NONE)
            {
                intf = data[offset + 2];
                alt = data[offset + 3];
            }
        }
        else if (matching && type == USB_DT_ENDPOINT &&
                 descriptor_length >= 7 && (data[offset + 3] & 3) == 2)
        {
            uint8_t endpoint = data[offset + 2];
            if (endpoint & USB_DIR_IN)
            {
                in = endpoint & 0xf;
                in_mps = get_le16(data + offset + 4) & 0x7ff;
            }
            else
            {
                out = endpoint & 0xf;
                out_mps = get_le16(data + offset + 4) & 0x7ff;
            }
        }
        offset += descriptor_length;
    }
    SAVE_MATCHED_INTERFACE();
#undef SAVE_MATCHED_INTERFACE
    return found_ipheth;
}

static void read_device_udid(uint8_t string_index)
{
    uint16_t language = 0;
    int result;
    int source;
    int target = 0;

    host.udid[0] = '\0';
    if (!string_index)
        return;
    result = control_transfer(USB_DIR_IN | USB_TYPE_STANDARD |
                              USB_RECIP_DEVICE, USB_REQ_GET_DESCRIPTOR,
                              USB_DT_STRING << 8, 0, control_buffer, 4);
    if (result >= 4)
        language = get_le16(control_buffer + 2);
    result = control_transfer(USB_DIR_IN | USB_TYPE_STANDARD |
                              USB_RECIP_DEVICE, USB_REQ_GET_DESCRIPTOR,
                              (USB_DT_STRING << 8) | string_index,
                              language, control_buffer, 82);
    if (result < 4 || control_buffer[1] != USB_DT_STRING)
        return;
    for (source = 2; source + 1 < result && target < 40; source += 2)
    {
        if (control_buffer[source + 1] != 0)
            return;
        host.udid[target++] = control_buffer[source];
    }
    host.udid[target] = '\0';
    if (target == 24)
    {
        memmove(host.udid + 9, host.udid + 8, 17);
        host.udid[8] = '-';
    }
}

static bool enumerate_iphone(void)
{
    struct usb_device_descriptor *device;
    int configuration_count;
    int result;
    int config;
    uint8_t serial_index;

    host.address = 0;
    result = control_transfer(USB_DIR_IN | USB_TYPE_STANDARD |
                              USB_RECIP_DEVICE, USB_REQ_GET_DESCRIPTOR,
                              USB_DT_DEVICE << 8, 0, control_buffer, 18);
    if (result != 18)
        return false;
    device = (struct usb_device_descriptor *)control_buffer;
    logf("ipheth device %04x:%04x cfgs=%d", device->idVendor,
         device->idProduct, device->bNumConfigurations);
    if (device->idVendor != APPLE_VENDOR_ID)
    {
        set_error("Connected USB device is not an iPhone");
        return false;
    }
    configuration_count = device->bNumConfigurations;
    serial_index = device->iSerialNumber;
    if (control_transfer(USB_DIR_OUT | USB_TYPE_STANDARD |
                         USB_RECIP_DEVICE, USB_REQ_SET_ADDRESS,
                         1, 0, control_buffer, 0) < 0)
        return false;
    host.address = 1;
    sleep(HZ / 100 + 1);
    read_device_udid(serial_index);

    /* Apple exposes several overlapping personalities.  Match usbmuxd's
     * preference for the highest usable configuration so tethering does not
     * accidentally select an older configuration without the multiplexor. */
    for (config = configuration_count - 1; config >= 0; config--)
    {
        struct usb_config_descriptor *descriptor;
        int total;
        result = control_transfer(USB_DIR_IN | USB_TYPE_STANDARD |
                                  USB_RECIP_DEVICE,
                                  USB_REQ_GET_DESCRIPTOR,
                                  (USB_DT_CONFIG << 8) | config, 0,
                                  control_buffer, 9);
        if (result != 9)
            continue;
        descriptor = (struct usb_config_descriptor *)control_buffer;
        total = get_le16(&descriptor->wTotalLength);
        if (total < 9 || total > HOST_CONFIG_MAX)
            continue;
        host.bulk_in = host.bulk_out = 0;
        host.bulk_in_mps = host.bulk_out_mps = 0;
        host.mux_bulk_in = host.mux_bulk_out = 0;
        host.mux_bulk_in_mps = host.mux_bulk_out_mps = 0;
        result = control_transfer(USB_DIR_IN | USB_TYPE_STANDARD |
                                  USB_RECIP_DEVICE,
                                  USB_REQ_GET_DESCRIPTOR,
                                  (USB_DT_CONFIG << 8) | config, 0,
                                  control_buffer, total);
        if (result != total || !parse_tether_interface(control_buffer, total))
            continue;
        host.configuration = descriptor->bConfigurationValue;
        break;
    }
    if (!host.configuration)
    {
        set_error("iPhone tether interface not found");
        return false;
    }
    logf("ipheth cfg=%d intf=%d alt=%d in=%d/%d out=%d/%d",
         host.configuration, host.interface_number,
         host.alternate_setting, host.bulk_in, host.bulk_in_mps,
         host.bulk_out, host.bulk_out_mps);
    logf("usbmux intf=%d alt=%d in=%d/%d out=%d/%d",
         host.mux_interface_number, host.mux_alternate_setting,
         host.mux_bulk_in, host.mux_bulk_in_mps,
         host.mux_bulk_out, host.mux_bulk_out_mps);
    if (control_transfer(USB_DIR_OUT | USB_TYPE_STANDARD |
                         USB_RECIP_DEVICE, USB_REQ_SET_CONFIGURATION,
                         host.configuration, 0, control_buffer, 0) < 0 ||
        control_transfer(USB_DIR_OUT | USB_TYPE_STANDARD |
                         USB_RECIP_INTERFACE, USB_REQ_SET_INTERFACE,
                         host.alternate_setting, host.interface_number,
                         control_buffer, 0) < 0)
        return false;
    if (host.mux_bulk_in &&
        control_transfer(USB_DIR_OUT | USB_TYPE_STANDARD |
                         USB_RECIP_INTERFACE, USB_REQ_SET_INTERFACE,
                         host.mux_alternate_setting,
                         host.mux_interface_number,
                         control_buffer, 0) < 0)
    {
        host.mux_bulk_in = 0;
        host.mux_bulk_out = 0;
    }
    result = control_transfer(USB_DIR_IN | USB_TYPE_VENDOR |
                              USB_RECIP_DEVICE, IPHETH_GET_MAC, 0,
                              host.interface_number, control_buffer, 64);
    if (result >= 6)
    {
        memcpy(host_mac, control_buffer, sizeof(host_mac));
        host.mac_valid = true;
    }
    host.in_toggle = HOST_PID_DATA0;
    host.out_toggle = HOST_PID_DATA0;
    host.mux_in_toggle = HOST_PID_DATA0;
    host.mux_out_toggle = HOST_PID_DATA0;
    return true;
}

static void poll_bulk_in(void)
{
    int result;
    int packets;
    if (host.frame_pending)
        return;
    result = channel_transfer(1, host.bulk_in, true, HCCHAR_TYPE_BULK,
                              host.in_toggle, transfer_buffer,
                              sizeof(transfer_buffer), host.bulk_in_mps);
    if (result == HOST_XFER_NAK)
        return;
    if (result < 0)
    {
        set_error("iPhone bulk receive failed");
        return;
    }
    /*
     * A successfully completed receive, including a zero-length packet, is
     * how Apple's interface confirms that the host pairing preflight has
     * completed.  The upstream ipheth driver deliberately makes the same
     * transition before resubmitting its receive URB.
     */
    host.paired_traffic_seen = true;
    if (result == 0)
        return;
    packets = (result + host.bulk_in_mps - 1) / host.bulk_in_mps;
    if (packets & 1)
        host.in_toggle = host.in_toggle == HOST_PID_DATA0 ?
                         HOST_PID_DATA1 : HOST_PID_DATA0;
    if (result == 4 && transfer_buffer[0] == 0 && transfer_buffer[1] == 1)
        return;
    if (result <= 2 || result - 2 > HOST_FRAME_MAX)
        return;
    host.frame_length = result - 2;
    memcpy(received_frame, transfer_buffer + 2, host.frame_length);
    host.frame_pending = true;
}

static void check_carrier(void)
{
    int result = control_transfer(USB_DIR_IN | USB_TYPE_VENDOR |
                                  USB_RECIP_DEVICE, IPHETH_CARRIER_CHECK,
                                  0, host.interface_number,
                                  control_buffer, 64);
    bool carrier = (result == 1 && control_buffer[0] == IPHETH_CARRIER_ON) ||
                   (result >= 2 && control_buffer[1] == IPHETH_CARRIER_ON);
    if (carrier)
    {
        host.state = USB_IPHONE_LINK;
        host.status = "iPhone Personal Hotspot link";
        logf("ipheth carrier on");
    }
    else
    {
        host.state = USB_IPHONE_WAITING_FOR_TRUST;
        host.status = "Unlock iPhone, enable hotspot, and tap Trust";
    }
}

void usb_iphone_tether_start(void)
{
    memset(&host, 0, sizeof(host));
    host.state = USB_IPHONE_WAITING;
    host.status = "Waiting for externally powered iPhone";
    host.connect_deadline = current_tick + 5 * HZ;
    logf("ipheth host start");
    usb_dw_target_disable_irq();
    usb_dw_target_enable_clocks();
    DWC_GAHBCFG = 0;
    if (!wait_register(&DWC_GRSTCTL, AHBIDL, AHBIDL, HZ / 2))
    {
        set_error("USB host AHB did not become idle");
        return;
    }
    DWC_GRSTCTL = CSRST;
    if (!wait_register(&DWC_GRSTCTL, CSRST, 0, HZ / 2))
    {
        set_error("USB host core reset timed out");
        return;
    }
    DWC_GUSBCFG = (DWC_GUSBCFG & ~FDMOD) | HOST_FORCE_MODE | TRDT(5);
    sleep(HZ / 40 + 1);
    if (!(DWC_GINTSTS & CMOD))
    {
        set_error("USB controller did not enter host mode");
        return;
    }
    DWC_GCCFG = PWRDWN | NOVBUSSENS;
    DWC_GAHBCFG = DMAEN | HBSTLEN(HBSTLEN_INCR8);
    DWC_GRSTCTL = TXFFLSH | TXFNUM(0x10);
    wait_register(&DWC_GRSTCTL, TXFFLSH, 0, HZ / 20 + 1);
    DWC_GRSTCTL = RXFFLSH;
    wait_register(&DWC_GRSTCTL, RXFFLSH, 0, HZ / 20 + 1);
    DWC_HCFG = 1;
    DWC_GRXFSIZ = 0x360;
    DWC_TX0FSIZ = (0x80u << 16) | 0x360;
    DWC_HPTXFSIZ = (0x3e0u << 16) | 0x100;
    DWC_HAINTMSK = 0;
    DWC_GINTMSK = 0;
    DWC_HPRT = HPRT_POWER;
}

void usb_iphone_tether_stop(void)
{
    int channel;
    for (channel = 0; channel < 3; channel++)
        channel_halt(channel);
    DWC_HPRT = 0;
    DWC_GAHBCFG = 0;
    usb_dw_target_disable_clocks();
    memset(&host, 0, sizeof(host));
    host.status = "Off";
    logf("ipheth host stop");
}

void usb_iphone_tether_service(void)
{
    if (host.state == USB_IPHONE_OFF || host.state == USB_IPHONE_ERROR)
        return;
    if (!(DWC_HPRT & HPRT_CONNECT))
    {
        host.state = USB_IPHONE_WAITING;
        host.status = TIME_BEFORE(current_tick, host.connect_deadline) ?
                      "Waiting for externally powered iPhone" :
                      "No USB host link - powered adapter required";
        host.configuration = 0;
        host.address = 0;
        return;
    }
    if (host.state == USB_IPHONE_WAITING)
    {
        host.state = USB_IPHONE_ENUMERATING;
        host.status = "Enumerating iPhone";
        if (!port_reset() || !enumerate_iphone())
        {
            if (host.state != USB_IPHONE_ERROR)
                set_error("iPhone USB enumeration failed");
            return;
        }
        host.state = USB_IPHONE_WAITING_FOR_TRUST;
        host.status = "Unlock iPhone, enable hotspot, and tap Trust";
        host.next_poll = current_tick;
    }
    if (!host.mac_valid && !TIME_BEFORE(current_tick, host.next_poll))
    {
        int result = control_transfer(USB_DIR_IN | USB_TYPE_VENDOR |
                                      USB_RECIP_DEVICE, IPHETH_GET_MAC, 0,
                                      host.interface_number,
                                      control_buffer, 64);
        if (result >= 6)
        {
            memcpy(host_mac, control_buffer, sizeof(host_mac));
            host.mac_valid = true;
        }
        host.next_poll = current_tick + HZ;
    }
    poll_bulk_in();
    if (host.paired_traffic_seen &&
        !TIME_BEFORE(current_tick, host.next_poll))
    {
        check_carrier();
        host.next_poll = current_tick + HZ;
    }
}

bool usb_iphone_tether_link_active(void)
{
    return host.state == USB_IPHONE_LINK;
}

enum usb_iphone_tether_state usb_iphone_tether_state(void)
{
    return host.state;
}

const char *usb_iphone_tether_status(void)
{
    return host.status ? host.status : "Off";
}

int usb_iphone_tether_send_frame(const void *frame, int length)
{
    int result;
    int packets;
    if (!usb_iphone_tether_link_active() || length < 14 ||
        length > HOST_FRAME_MAX)
        return -1;
    memcpy(transfer_buffer, frame, length);
    result = channel_transfer(2, host.bulk_out, false, HCCHAR_TYPE_BULK,
                              host.out_toggle, transfer_buffer, length,
                              host.bulk_out_mps);
    if (result != length)
        return -1;
    packets = (length + host.bulk_out_mps - 1) / host.bulk_out_mps;
    if (packets & 1)
        host.out_toggle = host.out_toggle == HOST_PID_DATA0 ?
                          HOST_PID_DATA1 : HOST_PID_DATA0;
    return length;
}

int usb_iphone_tether_receive_frame(void *frame, int capacity)
{
    int length;
    if (!host.frame_pending)
        return 0;
    if (capacity < host.frame_length)
    {
        host.frame_pending = false;
        return -1;
    }
    length = host.frame_length;
    memcpy(frame, received_frame, length);
    host.frame_pending = false;
    return length;
}

bool usb_iphone_tether_get_mac(unsigned char mac[6])
{
    if (!host.mac_valid)
        return false;
    memcpy(mac, host_mac, 6);
    return true;
}

bool usb_iphone_tether_get_udid(char *udid, int capacity)
{
    if (!udid || capacity <= 0 || !host.udid[0])
        return false;
    strlcpy(udid, host.udid, capacity);
    return true;
}

bool usb_iphone_tether_mux_available(void)
{
    return host.mux_bulk_in && host.mux_bulk_out &&
           host.state != USB_IPHONE_OFF && host.state != USB_IPHONE_ERROR;
}

int usb_iphone_tether_mux_send(const void *packet, int length)
{
    int result;
    int packets;
    if (!usb_iphone_tether_mux_available() || !packet || length <= 0)
        return -1;
    result = channel_transfer(3, host.mux_bulk_out, false, HCCHAR_TYPE_BULK,
                              host.mux_out_toggle, (void *)packet, length,
                              host.mux_bulk_out_mps);
    if (result != length)
        return -1;
    packets = (length + host.mux_bulk_out_mps - 1) /
              host.mux_bulk_out_mps;
    if (packets & 1)
        host.mux_out_toggle = host.mux_out_toggle == HOST_PID_DATA0 ?
                              HOST_PID_DATA1 : HOST_PID_DATA0;
    return length;
}

int usb_iphone_tether_mux_receive(void *packet, int capacity)
{
    int result;
    int packets;
    if (!usb_iphone_tether_mux_available() || !packet || capacity <= 0)
        return -1;
    result = channel_transfer(4, host.mux_bulk_in, true, HCCHAR_TYPE_BULK,
                              host.mux_in_toggle, packet, capacity,
                              host.mux_bulk_in_mps);
    if (result < 0)
        return result == HOST_XFER_NAK ? 0 : -1;
    packets = result ? (result + host.mux_bulk_in_mps - 1) /
                       host.mux_bulk_in_mps : 1;
    if (packets & 1)
        host.mux_in_toggle = host.mux_in_toggle == HOST_PID_DATA0 ?
                             HOST_PID_DATA1 : HOST_PID_DATA0;
    return result;
}
