/***************************************************************************
 * Rockbox USB CDC Network Control Model class driver
 *
 * Exposes ordinary Ethernet frames to a small fixed-memory network service.
 * No allocation or storage access occurs in the transfer path.
 ****************************************************************************/
#include "config.h"
#include "system.h"
#include "string.h"
#include "usb.h"
#include "usb_core.h"
#include "usb_drv.h"
#include "usb_ethernet.h"
#include "kernel.h"

#define CDC_SUBCLASS_NCM 0x0d
#define CDC_PROTOCOL_NONE 0x00
#define CDC_DATA_PROTOCOL_NCM 0x01
#define CDC_SUBTYPE_HEADER 0x00
#define CDC_SUBTYPE_UNION 0x06
#define CDC_SUBTYPE_ETHERNET 0x0f
#define CDC_SUBTYPE_NCM 0x1a
#define CDC_SET_ETHERNET_PACKET_FILTER 0x43
#define CDC_GET_NTB_PARAMETERS 0x80
#define CDC_GET_NTB_FORMAT 0x83
#define CDC_SET_NTB_FORMAT 0x84
#define CDC_GET_NTB_INPUT_SIZE 0x85
#define CDC_SET_NTB_INPUT_SIZE 0x86
#define CDC_GET_MAX_DATAGRAM_SIZE 0x87
#define CDC_SET_MAX_DATAGRAM_SIZE 0x88
#define CDC_GET_CRC_MODE 0x89
#define CDC_SET_CRC_MODE 0x8a
#define CDC_NOTIFY_NETWORK_CONNECTION 0x00
#define CDC_NOTIFY_SPEED_CHANGE 0x2a
#define NCM_NTB_MAX_SIZE 16384
#define NCM_NTH16_SIZE 12
#define NCM_NDP16_SIZE 16
#define NCM_NTH16_SIGNATURE 0x484d434e
#define NCM_NDP16_SIGNATURE 0x304d434e
#define ETH_TYPE_IPV4 0x0800
#define ETH_TYPE_ARP 0x0806
#define IP_PROTOCOL_ICMP 1
#define IP_PROTOCOL_UDP 17
#define DHCP_SERVER_PORT 67
#define DHCP_CLIENT_PORT 68
#define DHCP_MAGIC 0x63825363
#define DHCP_DISCOVER 1
#define DHCP_OFFER 2
#define DHCP_REQUEST 3
#define DHCP_ACK 5
#define UDP_QUEUE_SLOTS 12
#define UDP_PAYLOAD_MAX 1400
#define BROWSER_PORT 47702
#define ROCKPOD_IP 0x0a4d0002
#define HOST_IP 0x0a4d0001

struct cdc_header_descriptor {
    uint8_t length;
    uint8_t type;
    uint8_t subtype;
    uint16_t version;
} __attribute__((packed));

struct cdc_union_descriptor {
    uint8_t length;
    uint8_t type;
    uint8_t subtype;
    uint8_t control_interface;
    uint8_t data_interface;
} __attribute__((packed));

struct cdc_ethernet_descriptor {
    uint8_t length;
    uint8_t type;
    uint8_t subtype;
    uint8_t mac_string;
    uint32_t statistics;
    uint16_t max_segment_size;
    uint16_t multicast_filters;
    uint8_t power_filters;
} __attribute__((packed));

struct cdc_ncm_descriptor {
    uint8_t length;
    uint8_t type;
    uint8_t subtype;
    uint16_t version;
    uint8_t capabilities;
} __attribute__((packed));

static struct usb_interface_assoc_descriptor association = {
    .bLength = sizeof(struct usb_interface_assoc_descriptor),
    .bDescriptorType = USB_DT_INTERFACE_ASSOCIATION,
    .bInterfaceCount = 2,
    .bFunctionClass = USB_CLASS_COMM,
    .bFunctionSubClass = CDC_SUBCLASS_NCM,
    .bFunctionProtocol = CDC_PROTOCOL_NONE,
};

static struct usb_interface_descriptor control_interface_descriptor = {
    .bLength = sizeof(struct usb_interface_descriptor),
    .bDescriptorType = USB_DT_INTERFACE,
    .bNumEndpoints = 1,
    .bInterfaceClass = USB_CLASS_COMM,
    .bInterfaceSubClass = CDC_SUBCLASS_NCM,
    .bInterfaceProtocol = CDC_PROTOCOL_NONE,
};

static struct cdc_header_descriptor header_descriptor = {
    sizeof(struct cdc_header_descriptor), USB_DT_CS_INTERFACE,
    CDC_SUBTYPE_HEADER, 0x0110
};

static struct cdc_union_descriptor union_descriptor = {
    sizeof(struct cdc_union_descriptor), USB_DT_CS_INTERFACE,
    CDC_SUBTYPE_UNION, 0, 0
};

static struct cdc_ethernet_descriptor ethernet_descriptor = {
    sizeof(struct cdc_ethernet_descriptor), USB_DT_CS_INTERFACE,
    CDC_SUBTYPE_ETHERNET, USB_STRING_INDEX_ETHERNET_MAC, 0,
    USB_ETHERNET_FRAME_MAX, 0, 0
};

static struct cdc_ncm_descriptor ncm_descriptor = {
    sizeof(struct cdc_ncm_descriptor), USB_DT_CS_INTERFACE,
    CDC_SUBTYPE_NCM, 0x0100, 0
};

static struct usb_interface_descriptor data_alt0_descriptor = {
    .bLength = sizeof(struct usb_interface_descriptor),
    .bDescriptorType = USB_DT_INTERFACE,
    .bAlternateSetting = 0,
    .bNumEndpoints = 0,
    .bInterfaceClass = USB_CLASS_CDC_DATA,
    .bInterfaceProtocol = CDC_DATA_PROTOCOL_NCM,
};

static struct usb_interface_descriptor data_alt1_descriptor = {
    .bLength = sizeof(struct usb_interface_descriptor),
    .bDescriptorType = USB_DT_INTERFACE,
    .bAlternateSetting = 1,
    .bNumEndpoints = 2,
    .bInterfaceClass = USB_CLASS_CDC_DATA,
    .bInterfaceProtocol = CDC_DATA_PROTOCOL_NCM,
};

static struct usb_endpoint_descriptor endpoint_descriptor = {
    .bLength = sizeof(struct usb_endpoint_descriptor),
    .bDescriptorType = USB_DT_ENDPOINT,
};

struct usb_class_driver_ep_allocation usb_ethernet_ep_allocs[3] = {
    { USB_ENDPOINT_XFER_BULK, DIR_IN, 0, false },
    { USB_ENDPOINT_XFER_BULK, DIR_OUT, 0, false },
    { USB_ENDPOINT_XFER_INT, DIR_IN, 0, false },
};

#define EP_IN  (usb_ethernet_ep_allocs[0].ep)
#define EP_OUT (usb_ethernet_ep_allocs[1].ep)
#define EP_INT (usb_ethernet_ep_allocs[2].ep)

static int control_interface;
static int data_interface;
static int data_alt;
static volatile bool configured;
static volatile bool tx_busy;
static struct semaphore tx_complete_sem;
static unsigned char rx_buffer[NCM_NTB_MAX_SIZE]
    USB_DEVBSS_ATTR __attribute__((aligned(32)));
static unsigned char tx_buffer[NCM_NTB_MAX_SIZE]
    USB_DEVBSS_ATTR __attribute__((aligned(32)));
static unsigned char udp_tx_buffer[14 + 20 + 8 + UDP_PAYLOAD_MAX]
    USB_DEVBSS_ATTR __attribute__((aligned(32)));
static unsigned char link_notification[16]
    USB_DEVBSS_ATTR __attribute__((aligned(4)));
static const unsigned char local_mac[6] = { 0x02, 0x00, 0x00, 0x00, 0x00, 0x02 };
static unsigned char host_mac[6];
static volatile bool host_mac_valid;
static volatile bool notify_connect_pending;
static uint16_t ncm_sequence;
static unsigned char control_buffer[32]
    USB_DEVBSS_ATTR __attribute__((aligned(4)));

struct udp_message {
    uint16_t port;
    uint16_t length;
    unsigned char data[UDP_PAYLOAD_MAX];
};

static struct udp_message udp_queue[UDP_QUEUE_SLOTS];
static volatile unsigned int udp_read;
static volatile unsigned int udp_write;
static struct mutex udp_send_mutex;
static struct mutex udp_receive_mutex;

static uint16_t get_be16(const unsigned char *p)
{
    return ((uint16_t)p[0] << 8) | p[1];
}

static uint16_t get_le16(const unsigned char *p)
{
    return p[0] | ((uint16_t)p[1] << 8);
}

static uint32_t get_le32(const unsigned char *p)
{
    return p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) |
           ((uint32_t)p[3] << 24);
}

static uint32_t get_be32(const unsigned char *p)
{
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8) | p[3];
}

static void put_be16(unsigned char *p, uint16_t value)
{
    p[0] = value >> 8;
    p[1] = value;
}

static void put_be32(unsigned char *p, uint32_t value)
{
    p[0] = value >> 24;
    p[1] = value >> 16;
    p[2] = value >> 8;
    p[3] = value;
}

static void put_le16(unsigned char *p, uint16_t value)
{
    p[0] = value;
    p[1] = value >> 8;
}

static void put_le32(unsigned char *p, uint32_t value)
{
    p[0] = value;
    p[1] = value >> 8;
    p[2] = value >> 16;
    p[3] = value >> 24;
}

static uint16_t ip_checksum(const unsigned char *data, int length)
{
    uint32_t sum = 0;
    while (length > 1)
    {
        sum += get_be16(data);
        data += 2;
        length -= 2;
    }
    if (length)
        sum += (uint16_t)data[0] << 8;
    while (sum >> 16)
        sum = (sum & 0xffff) + (sum >> 16);
    return ~sum;
}

static int dhcp_message_type(const unsigned char *options, int length)
{
    int offset = 0;
    while (offset < length)
    {
        int type = options[offset++];
        int option_length;
        if (type == 0)
            continue;
        if (type == 255)
            break;
        if (offset >= length)
            return 0;
        option_length = options[offset++];
        if (option_length > length - offset)
            return 0;
        if (type == 53 && option_length == 1)
            return options[offset];
        offset += option_length;
    }
    return 0;
}

static void send_dhcp_reply(const unsigned char *frame, int length,
                            int reply_type)
{
    const unsigned char *request_ip = frame + 14;
    int request_header_length = (request_ip[0] & 0x0f) * 4;
    const unsigned char *request_udp = request_ip + request_header_length;
    const unsigned char *request_bootp = request_udp + 8;
    unsigned char *reply = udp_tx_buffer;
    unsigned char *ip = reply + 14;
    unsigned char *udp = ip + 20;
    unsigned char *bootp = udp + 8;
    unsigned char *options = bootp + 240;
    int option = 0;
    int bootp_length;

    if (length < 14 + request_header_length + 8 + 240 ||
        get_be32(request_bootp + 236) != DHCP_MAGIC ||
        request_bootp[0] != 1 || request_bootp[1] != 1 ||
        request_bootp[2] != 6)
        return;
    memset(reply, 0, sizeof(udp_tx_buffer));
    memcpy(reply, frame + 6, 6);
    memcpy(reply + 6, local_mac, 6);
    put_be16(reply + 12, ETH_TYPE_IPV4);
    ip[0] = 0x45;
    ip[8] = 64;
    ip[9] = IP_PROTOCOL_UDP;
    put_be32(ip + 12, ROCKPOD_IP);
    put_be32(ip + 16, 0xffffffff);
    put_be16(udp, DHCP_SERVER_PORT);
    put_be16(udp + 2, DHCP_CLIENT_PORT);
    bootp[0] = 2;
    bootp[1] = 1;
    bootp[2] = 6;
    memcpy(bootp + 4, request_bootp + 4, 4);
    memcpy(bootp + 10, request_bootp + 10, 2);
    put_be32(bootp + 16, HOST_IP);
    put_be32(bootp + 20, ROCKPOD_IP);
    memcpy(bootp + 28, request_bootp + 28, 16);
    put_be32(bootp + 236, DHCP_MAGIC);
    options[option++] = 53;
    options[option++] = 1;
    options[option++] = reply_type;
    options[option++] = 54;
    options[option++] = 4;
    put_be32(options + option, ROCKPOD_IP);
    option += 4;
    options[option++] = 1;
    options[option++] = 4;
    put_be32(options + option, 0xffffff00);
    option += 4;
    options[option++] = 51;
    options[option++] = 4;
    put_be32(options + option, 86400);
    option += 4;
    options[option++] = 255;
    bootp_length = 240 + option;
    put_be16(udp + 4, 8 + bootp_length);
    put_be16(ip + 2, 20 + 8 + bootp_length);
    put_be16(ip + 10, ip_checksum(ip, 20));
    usb_ethernet_send_frame(reply, 14 + 20 + 8 + bootp_length);
}

static bool handle_dhcp(const unsigned char *frame, int length,
                        int header_length, int total_length)
{
    const unsigned char *ip = frame + 14;
    const unsigned char *udp = ip + header_length;
    const unsigned char *bootp = udp + 8;
    int udp_length;
    int message;

    if (ip[9] != IP_PROTOCOL_UDP || total_length < header_length + 8 + 240 ||
        get_be16(udp) != DHCP_CLIENT_PORT ||
        get_be16(udp + 2) != DHCP_SERVER_PORT)
        return false;
    udp_length = get_be16(udp + 4);
    if (udp_length < 8 + 240 || udp_length > total_length - header_length ||
        get_be32(bootp + 236) != DHCP_MAGIC)
        return true;
    message = dhcp_message_type(bootp + 240, udp_length - 8 - 240);
    if (message == DHCP_DISCOVER)
        send_dhcp_reply(frame, length, DHCP_OFFER);
    else if (message == DHCP_REQUEST)
        send_dhcp_reply(frame, length, DHCP_ACK);
    return true;
}

static void ethernet_header(unsigned char *frame, uint16_t type)
{
    memcpy(frame, host_mac, 6);
    memcpy(frame + 6, local_mac, 6);
    put_be16(frame + 12, type);
}

static void handle_arp(const unsigned char *frame, int length)
{
    unsigned char reply[42];
    if (length < 42 || get_be16(frame + 20) != 1 ||
        get_be32(frame + 38) != ROCKPOD_IP)
        return;
    memcpy(host_mac, frame + 6, 6);
    host_mac_valid = true;
    ethernet_header(reply, ETH_TYPE_ARP);
    put_be16(reply + 14, 1);
    put_be16(reply + 16, ETH_TYPE_IPV4);
    reply[18] = 6;
    reply[19] = 4;
    put_be16(reply + 20, 2);
    memcpy(reply + 22, local_mac, 6);
    put_be32(reply + 28, ROCKPOD_IP);
    memcpy(reply + 32, frame + 22, 6);
    memcpy(reply + 38, frame + 28, 4);
    usb_ethernet_send_frame(reply, sizeof(reply));
}

static void queue_udp(uint16_t port, const unsigned char *data, int length)
{
    unsigned int next = (udp_write + 1) % UDP_QUEUE_SLOTS;
    struct udp_message *message;

    /* The host opens the browser flow before Safari sends its request so a
     * normal stateful firewall accepts the return packet.  Keep only one
     * such probe queued while Safari is closed; otherwise periodic probes
     * could consume the small fixed UDP queue. */
    if (port == BROWSER_PORT && length == 5 &&
        !memcmp(data, "RPB1\0", 5))
    {
        unsigned int cursor = udp_read;
        while (cursor != udp_write)
        {
            message = &udp_queue[cursor];
            if (message->port == port && message->length == length &&
                !memcmp(message->data, data, length))
                return;
            cursor = (cursor + 1) % UDP_QUEUE_SLOTS;
        }
    }
    if (next == udp_read || length < 0 || length > UDP_PAYLOAD_MAX)
        return;
    message = &udp_queue[udp_write];
    message->port = port;
    message->length = length;
    memcpy(message->data, data, length);
    udp_write = next;
}

static void handle_ipv4(const unsigned char *frame, int length)
{
    int header_length;
    int total_length;
    const unsigned char *ip = frame + 14;
    if (length < 34 || (ip[0] >> 4) != 4)
        return;
    header_length = (ip[0] & 0x0f) * 4;
    total_length = get_be16(ip + 2);
    if (header_length < 20 || total_length < header_length ||
        14 + total_length > length)
        return;
    if (handle_dhcp(frame, length, header_length, total_length))
        return;
    if (get_be32(ip + 16) != ROCKPOD_IP)
        return;
    memcpy(host_mac, frame + 6, 6);
    host_mac_valid = true;
    if (ip[9] == IP_PROTOCOL_UDP && total_length >= header_length + 8)
    {
        const unsigned char *udp = ip + header_length;
        int udp_length = get_be16(udp + 4);
        if (udp_length >= 8 && udp_length <= total_length - header_length)
            queue_udp(get_be16(udp + 2), udp + 8, udp_length - 8);
    }
    else if (ip[9] == IP_PROTOCOL_ICMP && total_length >= header_length + 8 &&
             ip[header_length] == 8 && !tx_busy)
    {
        unsigned char *reply = tx_buffer;
        int frame_length = 14 + total_length;
        memcpy(reply, frame, frame_length);
        memcpy(reply, frame + 6, 6);
        memcpy(reply + 6, local_mac, 6);
        memcpy(reply + 26, ip + 16, 4);
        put_be32(reply + 30, ROCKPOD_IP);
        reply[14 + header_length] = 0;
        put_be16(reply + 14 + header_length + 2, 0);
        put_be16(reply + 14 + header_length + 2,
                 ip_checksum(reply + 14 + header_length,
                             total_length - header_length));
        put_be16(reply + 24, 0);
        put_be16(reply + 24, ip_checksum(reply + 14, header_length));
        usb_ethernet_send_frame(reply, frame_length);
    }
}

static void handle_frame(const unsigned char *frame, int length)
{
    uint16_t type;
    if (length < 14)
        return;
    type = get_be16(frame + 12);
    if (type == ETH_TYPE_ARP)
        handle_arp(frame, length);
    else if (type == ETH_TYPE_IPV4)
        handle_ipv4(frame, length);
}

static void handle_ntb(const unsigned char *ntb, int length)
{
    unsigned int ndp_index;
    unsigned int block_length;
    int ndp_count = 0;

    if (length < NCM_NTH16_SIZE || get_le32(ntb) != NCM_NTH16_SIGNATURE ||
        get_le16(ntb + 4) != NCM_NTH16_SIZE)
        return;

    block_length = get_le16(ntb + 8);
    ndp_index = get_le16(ntb + 10);
    if (block_length > (unsigned int)length ||
        block_length > NCM_NTB_MAX_SIZE || block_length < NCM_NTH16_SIZE)
        return;

    while (ndp_index && ndp_count++ < 8)
    {
        unsigned int ndp_length;
        unsigned int entry;
        unsigned int next_ndp;

        if ((ndp_index & 3) || ndp_index < NCM_NTH16_SIZE ||
            ndp_index + 8 > block_length ||
            get_le32(ntb + ndp_index) != NCM_NDP16_SIGNATURE)
            return;
        ndp_length = get_le16(ntb + ndp_index + 4);
        next_ndp = get_le16(ntb + ndp_index + 6);
        if (ndp_length < 16 || (ndp_length & 3) ||
            ndp_index + ndp_length > block_length)
            return;

        for (entry = ndp_index + 8;
             entry + 4 <= ndp_index + ndp_length; entry += 4)
        {
            unsigned int frame_index = get_le16(ntb + entry);
            unsigned int frame_length = get_le16(ntb + entry + 2);
            if (frame_index == 0 && frame_length == 0)
                break;
            if (frame_index < NCM_NTH16_SIZE || frame_length < 14 ||
                frame_length > USB_ETHERNET_FRAME_MAX ||
                frame_index + frame_length > block_length)
                return;
            handle_frame(ntb + frame_index, frame_length);
        }
        ndp_index = next_ndp;
    }
}

static void prime_receive(void)
{
    if (configured && data_alt == 1)
        usb_drv_recv_nonblocking(EP_OUT, rx_buffer, sizeof(rx_buffer));
}

static void notify_link(void)
{
    memset(link_notification, 0, sizeof(link_notification));
    link_notification[0] = USB_DIR_IN | USB_TYPE_CLASS |
                           USB_RECIP_INTERFACE;
    put_le16(link_notification + 4, control_interface);
    if (data_alt == 1)
    {
        link_notification[1] = CDC_NOTIFY_SPEED_CHANGE;
        put_le16(link_notification + 6, 8);
        put_le32(link_notification + 8, 480000000);
        put_le32(link_notification + 12, 480000000);
        notify_connect_pending = true;
        usb_drv_send_nonblocking(EP_INT, link_notification, 16);
    }
    else
    {
        link_notification[1] = CDC_NOTIFY_NETWORK_CONNECTION;
        notify_connect_pending = false;
        usb_drv_send_nonblocking(EP_INT, link_notification, 8);
    }
}

static void notify_connected(void)
{
    memset(link_notification, 0, 8);
    link_notification[0] = USB_DIR_IN | USB_TYPE_CLASS |
                           USB_RECIP_INTERFACE;
    link_notification[1] = CDC_NOTIFY_NETWORK_CONNECTION;
    put_le16(link_notification + 2, data_alt == 1);
    put_le16(link_notification + 4, control_interface);
    usb_drv_send_nonblocking(EP_INT, link_notification, 8);
}

int usb_ethernet_set_first_interface(int interface)
{
    control_interface = interface;
    data_interface = interface + 1;
    return interface + 2;
}

int usb_ethernet_get_config_descriptor(unsigned char *dest,
                                       int max_packet_size)
{
    unsigned char *start = dest;
    association.bFirstInterface = control_interface;
    control_interface_descriptor.bInterfaceNumber = control_interface;
    union_descriptor.control_interface = control_interface;
    union_descriptor.data_interface = data_interface;
    data_alt0_descriptor.bInterfaceNumber = data_interface;
    data_alt1_descriptor.bInterfaceNumber = data_interface;

    PACK_DATA(&dest, association);
    PACK_DATA(&dest, control_interface_descriptor);
    PACK_DATA(&dest, header_descriptor);
    PACK_DATA(&dest, union_descriptor);
    PACK_DATA(&dest, ethernet_descriptor);
    PACK_DATA(&dest, ncm_descriptor);
    endpoint_descriptor.bEndpointAddress = EP_INT;
    endpoint_descriptor.bmAttributes = USB_ENDPOINT_XFER_INT;
    endpoint_descriptor.wMaxPacketSize = 16;
    endpoint_descriptor.bInterval = 9;
    PACK_DATA(&dest, endpoint_descriptor);
    PACK_DATA(&dest, data_alt0_descriptor);
    PACK_DATA(&dest, data_alt1_descriptor);
    endpoint_descriptor.bEndpointAddress = EP_IN;
    endpoint_descriptor.bmAttributes = USB_ENDPOINT_XFER_BULK;
    endpoint_descriptor.wMaxPacketSize = max_packet_size;
    endpoint_descriptor.bInterval = 0;
    PACK_DATA(&dest, endpoint_descriptor);
    endpoint_descriptor.bEndpointAddress = EP_OUT;
    PACK_DATA(&dest, endpoint_descriptor);
    return dest - start;
}

void usb_ethernet_init(void)
{
    configured = false;
    data_alt = 0;
    tx_busy = false;
    host_mac_valid = false;
    notify_connect_pending = false;
    ncm_sequence = 0;
    udp_read = udp_write = 0;
    mutex_init(&udp_send_mutex);
    mutex_init(&udp_receive_mutex);
    semaphore_init(&tx_complete_sem, 1, 0);
}

void usb_ethernet_init_connection(void)
{
    configured = true;
    data_alt = 0;
    tx_busy = false;
    notify_connect_pending = false;
}

void usb_ethernet_disconnect(void)
{
    configured = false;
    data_alt = 0;
    tx_busy = false;
    notify_connect_pending = false;
    semaphore_release(&tx_complete_sem);
}

int usb_ethernet_set_interface(int interface, int alt_setting)
{
    if (interface != data_interface || (alt_setting != 0 && alt_setting != 1))
        return -1;
    data_alt = alt_setting;
    if (data_alt == 1)
        prime_receive();
    notify_link();
    return 0;
}

int usb_ethernet_get_interface(int interface)
{
    if (interface == control_interface)
        return 0;
    return interface == data_interface ? data_alt : -1;
}

bool usb_ethernet_control_request(struct usb_ctrlrequest *req, void *reqdata,
                                  unsigned char *dest)
{
    (void)dest;
    if (req->wIndex != control_interface)
        return false;

    if (req->bRequestType == (USB_DIR_OUT | USB_TYPE_CLASS |
                              USB_RECIP_INTERFACE))
    {
        if (req->bRequest == CDC_SET_ETHERNET_PACKET_FILTER &&
            req->wLength == 0)
        {
            usb_drv_control_response(USB_CONTROL_ACK, NULL, 0);
            return true;
        }
        if (req->bRequest == CDC_SET_NTB_FORMAT && req->wLength == 0 &&
            req->wValue == 0)
        {
            usb_drv_control_response(USB_CONTROL_ACK, NULL, 0);
            return true;
        }
        if (req->bRequest == CDC_SET_CRC_MODE && req->wLength == 0 &&
            req->wValue == 0)
        {
            usb_drv_control_response(USB_CONTROL_ACK, NULL, 0);
            return true;
        }
        if ((req->bRequest == CDC_SET_NTB_INPUT_SIZE && req->wLength == 4) ||
            (req->bRequest == CDC_SET_MAX_DATAGRAM_SIZE && req->wLength == 2))
        {
            if (!reqdata)
                usb_drv_control_response(USB_CONTROL_RECEIVE, control_buffer,
                                         req->wLength);
            else if ((req->bRequest == CDC_SET_NTB_INPUT_SIZE &&
                      get_le32(reqdata) >= NCM_NTH16_SIZE + NCM_NDP16_SIZE &&
                      get_le32(reqdata) <= NCM_NTB_MAX_SIZE) ||
                     (req->bRequest == CDC_SET_MAX_DATAGRAM_SIZE &&
                      get_le16(reqdata) >= 14 &&
                      get_le16(reqdata) <= USB_ETHERNET_FRAME_MAX))
                usb_drv_control_response(USB_CONTROL_ACK, NULL, 0);
            else
                usb_drv_control_response(USB_CONTROL_STALL, NULL, 0);
            return true;
        }
    }
    else if (req->bRequestType == (USB_DIR_IN | USB_TYPE_CLASS |
                                   USB_RECIP_INTERFACE))
    {
        int response_length = 0;
        memset(control_buffer, 0, sizeof(control_buffer));
        if (req->bRequest == CDC_GET_NTB_PARAMETERS)
        {
            put_le16(control_buffer, 28);
            put_le16(control_buffer + 2, 1);
            put_le32(control_buffer + 4, NCM_NTB_MAX_SIZE);
            put_le16(control_buffer + 8, 4);
            put_le16(control_buffer + 12, 4);
            put_le32(control_buffer + 16, NCM_NTB_MAX_SIZE);
            put_le16(control_buffer + 20, 4);
            put_le16(control_buffer + 24, 4);
            put_le16(control_buffer + 26, 1);
            response_length = 28;
        }
        else if (req->bRequest == CDC_GET_NTB_INPUT_SIZE)
        {
            put_le32(control_buffer, NCM_NTB_MAX_SIZE);
            response_length = 4;
        }
        else if (req->bRequest == CDC_GET_MAX_DATAGRAM_SIZE)
        {
            put_le16(control_buffer, USB_ETHERNET_FRAME_MAX);
            response_length = 2;
        }
        else if (req->bRequest == CDC_GET_NTB_FORMAT ||
                 req->bRequest == CDC_GET_CRC_MODE)
        {
            put_le16(control_buffer, 0);
            response_length = 2;
        }
        if (response_length)
        {
            usb_drv_control_response(USB_CONTROL_ACK, control_buffer,
                                     MIN(response_length, req->wLength));
            return true;
        }
    }
    return false;
}

bool usb_ethernet_link_active(void)
{
    return configured && data_alt == 1;
}

bool usb_ethernet_wait_for_tx(int timeout)
{
    /* Consume a completion that arrived before this thread reached the wait.
     * A queued waiter is woken directly, so no token remains in that case. */
    if (!tx_busy)
    {
        semaphore_wait(&tx_complete_sem, TIMEOUT_NOBLOCK);
        return true;
    }
    if (semaphore_wait(&tx_complete_sem, timeout) != OBJ_WAIT_SUCCEEDED)
        return !tx_busy;
    return !tx_busy;
}

int usb_ethernet_send_frame(const void *frame, int length)
{
    int ndp_index;
    int ntb_length;

    if (!usb_ethernet_link_active() || tx_busy || length < 14 ||
        length > USB_ETHERNET_FRAME_MAX)
        return -1;
    if (frame == tx_buffer)
        memmove(tx_buffer + NCM_NTH16_SIZE, tx_buffer, length);
    else
        memcpy(tx_buffer + NCM_NTH16_SIZE, frame, length);

    ndp_index = (NCM_NTH16_SIZE + length + 3) & ~3;
    ntb_length = ndp_index + NCM_NDP16_SIZE;
    memset(tx_buffer + NCM_NTH16_SIZE + length, 0,
           ntb_length - NCM_NTH16_SIZE - length);
    put_le32(tx_buffer, NCM_NTH16_SIGNATURE);
    put_le16(tx_buffer + 4, NCM_NTH16_SIZE);
    put_le16(tx_buffer + 6, ncm_sequence++);
    put_le16(tx_buffer + 8, ntb_length);
    put_le16(tx_buffer + 10, ndp_index);
    put_le32(tx_buffer + ndp_index, NCM_NDP16_SIGNATURE);
    put_le16(tx_buffer + ndp_index + 4, NCM_NDP16_SIZE);
    put_le16(tx_buffer + ndp_index + 6, 0);
    put_le16(tx_buffer + ndp_index + 8, NCM_NTH16_SIZE);
    put_le16(tx_buffer + ndp_index + 10, length);
    put_le16(tx_buffer + ndp_index + 12, 0);
    put_le16(tx_buffer + ndp_index + 14, 0);
    tx_busy = true;
    if (usb_drv_send_nonblocking(EP_IN, tx_buffer, ntb_length) < 0)
    {
        tx_busy = false;
        return -1;
    }
    return length;
}

int usb_ethernet_udp_send(uint16_t port, const void *data, int length)
{
    unsigned char *frame = udp_tx_buffer;
    unsigned char *ip = frame + 14;
    unsigned char *udp = ip + 20;
    static uint16_t identification;
    int frame_length;
    int result;
    if (!host_mac_valid || length < 0 || length > UDP_PAYLOAD_MAX)
        return -1;
    mutex_lock(&udp_send_mutex);
    if (!host_mac_valid || !usb_ethernet_link_active() || tx_busy)
    {
        mutex_unlock(&udp_send_mutex);
        return -1;
    }
    frame_length = 14 + 20 + 8 + length;
    ethernet_header(frame, ETH_TYPE_IPV4);
    memset(ip, 0, 28);
    ip[0] = 0x45;
    put_be16(ip + 2, 20 + 8 + length);
    put_be16(ip + 4, ++identification);
    put_be16(ip + 6, 0x4000);
    ip[8] = 64;
    ip[9] = IP_PROTOCOL_UDP;
    put_be32(ip + 12, ROCKPOD_IP);
    put_be32(ip + 16, HOST_IP);
    put_be16(ip + 10, ip_checksum(ip, 20));
    put_be16(udp, port);
    put_be16(udp + 2, port);
    put_be16(udp + 4, 8 + length);
    put_be16(udp + 6, 0);
    memcpy(udp + 8, data, length);
    result = usb_ethernet_send_frame(frame, frame_length) < 0 ? -1 : length;
    mutex_unlock(&udp_send_mutex);
    return result;
}

int usb_ethernet_udp_send_batch(
    uint16_t port, const struct usb_ethernet_udp_datagram *datagrams,
    int count)
{
    static uint16_t identification;
    uint16_t frame_indexes[USB_ETHERNET_UDP_BATCH_MAX];
    uint16_t frame_lengths[USB_ETHERNET_UDP_BATCH_MAX];
    int cursor = NCM_NTH16_SIZE;
    int ndp_index;
    int ndp_length;
    int ntb_length;
    int i;

    if (!host_mac_valid || datagrams == NULL || count < 1 ||
        count > USB_ETHERNET_UDP_BATCH_MAX)
        return -1;

    mutex_lock(&udp_send_mutex);
    if (!host_mac_valid || !usb_ethernet_link_active() || tx_busy)
    {
        mutex_unlock(&udp_send_mutex);
        return -1;
    }

    for (i = 0; i < count; ++i)
    {
        unsigned char *frame;
        unsigned char *ip;
        unsigned char *udp;
        int payload_length = datagrams[i].length;
        int frame_length = 14 + 20 + 8 + payload_length;

        if (datagrams[i].data == NULL || payload_length < 0 ||
            payload_length > UDP_PAYLOAD_MAX)
            goto fail;
        cursor = (cursor + 3) & ~3;
        if (cursor + frame_length > NCM_NTB_MAX_SIZE)
            goto fail;
        frame_indexes[i] = cursor;
        frame_lengths[i] = frame_length;
        frame = tx_buffer + cursor;
        ip = frame + 14;
        udp = ip + 20;
        ethernet_header(frame, ETH_TYPE_IPV4);
        memset(ip, 0, 28);
        ip[0] = 0x45;
        put_be16(ip + 2, 20 + 8 + payload_length);
        put_be16(ip + 4, ++identification);
        put_be16(ip + 6, 0x4000);
        ip[8] = 64;
        ip[9] = IP_PROTOCOL_UDP;
        put_be32(ip + 12, ROCKPOD_IP);
        put_be32(ip + 16, HOST_IP);
        put_be16(ip + 10, ip_checksum(ip, 20));
        put_be16(udp, port);
        put_be16(udp + 2, port);
        put_be16(udp + 4, 8 + payload_length);
        put_be16(udp + 6, 0);
        memcpy(udp + 8, datagrams[i].data, payload_length);
        cursor += frame_length;
    }

    ndp_index = (cursor + 3) & ~3;
    ndp_length = 8 + (count + 1) * 4;
    ntb_length = ndp_index + ndp_length;
    if (ntb_length > NCM_NTB_MAX_SIZE)
        goto fail;
    memset(tx_buffer + cursor, 0, ntb_length - cursor);
    put_le32(tx_buffer, NCM_NTH16_SIGNATURE);
    put_le16(tx_buffer + 4, NCM_NTH16_SIZE);
    put_le16(tx_buffer + 6, ncm_sequence++);
    put_le16(tx_buffer + 8, ntb_length);
    put_le16(tx_buffer + 10, ndp_index);
    put_le32(tx_buffer + ndp_index, NCM_NDP16_SIGNATURE);
    put_le16(tx_buffer + ndp_index + 4, ndp_length);
    put_le16(tx_buffer + ndp_index + 6, 0);
    for (i = 0; i < count; ++i)
    {
        put_le16(tx_buffer + ndp_index + 8 + i * 4, frame_indexes[i]);
        put_le16(tx_buffer + ndp_index + 10 + i * 4, frame_lengths[i]);
    }
    put_le16(tx_buffer + ndp_index + 8 + count * 4, 0);
    put_le16(tx_buffer + ndp_index + 10 + count * 4, 0);

    tx_busy = true;
    if (usb_drv_send_nonblocking(EP_IN, tx_buffer, ntb_length) < 0)
    {
        tx_busy = false;
        goto fail;
    }
    mutex_unlock(&udp_send_mutex);
    return count;

fail:
    mutex_unlock(&udp_send_mutex);
    return -1;
}

int usb_ethernet_udp_receive(uint16_t port, void *data, int capacity)
{
    unsigned int cursor;
    int result = 0;

    mutex_lock(&udp_receive_mutex);
    cursor = udp_read;
    while (cursor != udp_write)
    {
        struct udp_message *message = &udp_queue[cursor];
        if (message->port == port)
        {
            int length = MIN((int)message->length, capacity);
            memcpy(data, message->data, length);
            while (cursor != udp_read)
            {
                unsigned int previous = (cursor + UDP_QUEUE_SLOTS - 1) %
                                        UDP_QUEUE_SLOTS;
                udp_queue[cursor] = udp_queue[previous];
                cursor = previous;
            }
            udp_read = (udp_read + 1) % UDP_QUEUE_SLOTS;
            result = length;
            break;
        }
        cursor = (cursor + 1) % UDP_QUEUE_SLOTS;
    }
    mutex_unlock(&udp_receive_mutex);
    return result;
}

void usb_ethernet_transfer_complete(int ep, int dir, int status, int length)
{
    if (dir == USB_DIR_OUT && ep == EP_NUM(EP_OUT))
    {
        if (status == 0 && length >= NCM_NTH16_SIZE &&
            length <= (int)sizeof(rx_buffer))
            handle_ntb(rx_buffer, length);
        prime_receive();
    }
    else if (dir == USB_DIR_IN && ep == EP_NUM(EP_IN))
    {
        tx_busy = false;
        semaphore_release(&tx_complete_sem);
    }
    else if (dir == USB_DIR_IN && ep == EP_NUM(EP_INT) &&
             notify_connect_pending)
    {
        notify_connect_pending = false;
        if (status == 0 && configured && data_alt == 1)
            notify_connected();
    }
}
