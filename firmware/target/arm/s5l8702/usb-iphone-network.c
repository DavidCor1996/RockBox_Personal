/***************************************************************************
 * Bounded IPv4/ARP/UDP client for Apple ipheth connectivity.
 *
 * DHCP qualifies the route; ARP and a small UDP queue provide the reusable
 * transport needed by DNS and higher layers. All packet storage is fixed and
 * all parsing is bounded.
 ****************************************************************************/
#include "config.h"
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include "kernel.h"
#include "system.h"
#include "usb_iphone_network.h"
#include "usb_iphone_tether.h"

#define ETH_IPV4                       0x0800
#define ETH_ARP                        0x0806
#define IP_ICMP                        1
#define IP_UDP                         17
#define DHCP_CLIENT_PORT               68
#define DHCP_SERVER_PORT               67
#define DHCP_MAGIC                     0x63825363
#define DHCP_DISCOVER                  1
#define DHCP_OFFER                     2
#define DHCP_REQUEST                   3
#define DHCP_ACK                       5
#define DHCP_NAK                       6
#define DHCP_OPT_SUBNET                1
#define DHCP_OPT_ROUTER                3
#define DHCP_OPT_DNS                   6
#define DHCP_OPT_REQUESTED_IP          50
#define DHCP_OPT_LEASE                 51
#define DHCP_OPT_MESSAGE               53
#define DHCP_OPT_SERVER                54
#define DHCP_OPT_PARAMETER_LIST        55
#define DHCP_OPT_END                   255
#define DHCP_FIXED_SIZE                240
#define DHCP_PACKET_MAX                600
#define DHCP_RETRY_TICKS               (3 * HZ)
#define DHCP_MAX_RETRIES               5
#define UDP_QUEUE_SLOTS                4
#define UDP_PAYLOAD_MAX                512
#define ARP_RETRY_TICKS                HZ

enum dhcp_state
{
    DHCP_OFF = 0,
    DHCP_DISCOVERING,
    DHCP_REQUESTING,
    DHCP_BOUND,
    DHCP_FAILED
};

struct iphone_network
{
    enum dhcp_state state;
    uint32_t xid;
    uint32_t address;
    uint32_t offered_address;
    uint32_t server;
    uint32_t subnet;
    uint32_t router;
    uint32_t dns;
    uint32_t lease_seconds;
    long retry_at;
    long lease_at;
    unsigned int retries;
    bool previous_transport;
    unsigned char mac[6];
    unsigned char peer_mac[6];
    uint32_t peer_ip;
    long arp_retry_at;
    bool peer_mac_valid;
    const char *status;
};

struct udp_message
{
    uint16_t port;
    uint16_t length;
    uint32_t source;
    unsigned char data[UDP_PAYLOAD_MAX];
};

static struct iphone_network network;
static unsigned char packet[DHCP_PACKET_MAX] CACHEALIGN_ATTR;
static struct udp_message udp_queue[UDP_QUEUE_SLOTS];
static unsigned int udp_read;
static unsigned int udp_write;

static uint16_t get_be16(const unsigned char *p)
{
    return ((uint16_t)p[0] << 8) | p[1];
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

static uint16_t checksum(const unsigned char *data, int length)
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

static void ethernet_header(unsigned char *frame,
                            const unsigned char destination[6],
                            uint16_t type)
{
    memcpy(frame, destination, 6);
    memcpy(frame + 6, network.mac, 6);
    put_be16(frame + 12, type);
}

static bool send_arp(uint32_t target)
{
    static const unsigned char broadcast[6] = {
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff
    };
    memset(packet, 0, 42);
    ethernet_header(packet, broadcast, ETH_ARP);
    put_be16(packet + 14, 1);
    put_be16(packet + 16, ETH_IPV4);
    packet[18] = 6;
    packet[19] = 4;
    put_be16(packet + 20, 1);
    memcpy(packet + 22, network.mac, 6);
    put_be32(packet + 28, network.address);
    put_be32(packet + 38, target);
    return usb_iphone_tether_send_frame(packet, 42) == 42;
}

static void handle_arp(const unsigned char *frame, int length)
{
    uint16_t operation;
    uint32_t sender;
    uint32_t target;
    unsigned char reply[42] CACHEALIGN_ATTR;

    if (length < 42 || get_be16(frame + 14) != 1 ||
        get_be16(frame + 16) != ETH_IPV4 || frame[18] != 6 ||
        frame[19] != 4)
        return;
    operation = get_be16(frame + 20);
    sender = get_be32(frame + 28);
    target = get_be32(frame + 38);
    if (sender)
    {
        network.peer_ip = sender;
        memcpy(network.peer_mac, frame + 22, 6);
        network.peer_mac_valid = true;
    }
    if (operation != 1 || target != network.address)
        return;
    memset(reply, 0, sizeof(reply));
    ethernet_header(reply, frame + 22, ETH_ARP);
    put_be16(reply + 14, 1);
    put_be16(reply + 16, ETH_IPV4);
    reply[18] = 6;
    reply[19] = 4;
    put_be16(reply + 20, 2);
    memcpy(reply + 22, network.mac, 6);
    put_be32(reply + 28, network.address);
    memcpy(reply + 32, frame + 22, 6);
    put_be32(reply + 38, sender);
    usb_iphone_tether_send_frame(reply, sizeof(reply));
}

static void queue_udp(uint16_t port, uint32_t source,
                      const unsigned char *data, int length)
{
    unsigned int next = (udp_write + 1) % UDP_QUEUE_SLOTS;
    struct udp_message *message;
    if (next == udp_read || length < 0 || length > UDP_PAYLOAD_MAX)
        return;
    message = &udp_queue[udp_write];
    message->port = port;
    message->source = source;
    message->length = length;
    memcpy(message->data, data, length);
    udp_write = next;
}

static int add_option(unsigned char *options, int offset, int capacity,
                      int type, const void *data, int length)
{
    if (length < 0 || length > 255 || offset + length + 2 > capacity)
        return -1;
    options[offset++] = type;
    options[offset++] = length;
    memcpy(options + offset, data, length);
    return offset + length;
}

static void begin_discovery(void)
{
    network.xid = ((uint32_t)current_tick << 16) ^
                  ((uint32_t)network.mac[4] << 8) ^ network.mac[5] ^
                  0x52504f44;
    if (!network.xid)
        network.xid = 0x52504f44;
    network.state = DHCP_DISCOVERING;
    network.retries = 0;
    network.retry_at = current_tick;
    network.status = "Requesting iPhone hotspot address";
}

static bool send_dhcp(int message_type)
{
    unsigned char *ip = packet + 14;
    unsigned char *udp = ip + 20;
    unsigned char *bootp = udp + 8;
    unsigned char *options = bootp + DHCP_FIXED_SIZE;
    unsigned char parameter_list[] = {
        DHCP_OPT_SUBNET, DHCP_OPT_ROUTER, DHCP_OPT_DNS, DHCP_OPT_LEASE
    };
    unsigned char message = message_type;
    int option = 0;
    int dhcp_length;
    int frame_length;

    memset(packet, 0, sizeof(packet));
    memset(packet, 0xff, 6);
    memcpy(packet + 6, network.mac, 6);
    put_be16(packet + 12, ETH_IPV4);
    ip[0] = 0x45;
    ip[8] = 64;
    ip[9] = IP_UDP;
    put_be32(ip + 16, 0xffffffff);
    put_be16(udp, DHCP_CLIENT_PORT);
    put_be16(udp + 2, DHCP_SERVER_PORT);

    bootp[0] = 1;
    bootp[1] = 1;
    bootp[2] = 6;
    put_be32(bootp + 4, network.xid);
    put_be16(bootp + 10, 0x8000);
    memcpy(bootp + 28, network.mac, 6);
    put_be32(bootp + 236, DHCP_MAGIC);
    option = add_option(options, option, sizeof(packet) -
                        (options - packet), DHCP_OPT_MESSAGE,
                        &message, 1);
    if (option < 0)
        return false;
    if (message_type == DHCP_REQUEST)
    {
        unsigned char value[4];
        put_be32(value, network.offered_address);
        option = add_option(options, option, sizeof(packet) -
                            (options - packet), DHCP_OPT_REQUESTED_IP,
                            value, sizeof(value));
        put_be32(value, network.server);
        option = add_option(options, option, sizeof(packet) -
                            (options - packet), DHCP_OPT_SERVER,
                            value, sizeof(value));
    }
    option = add_option(options, option, sizeof(packet) -
                        (options - packet), DHCP_OPT_PARAMETER_LIST,
                        parameter_list, sizeof(parameter_list));
    if (option < 0 || option + 1 > (int)(sizeof(packet) -
                                         (options - packet)))
        return false;
    options[option++] = DHCP_OPT_END;
    dhcp_length = DHCP_FIXED_SIZE + option;
    put_be16(udp + 4, 8 + dhcp_length);
    put_be16(ip + 2, 20 + 8 + dhcp_length);
    put_be16(ip + 10, checksum(ip, 20));
    frame_length = 14 + 20 + 8 + dhcp_length;
    return usb_iphone_tether_send_frame(packet, frame_length) == frame_length;
}

static bool option_u32(const unsigned char *options, int length,
                       int wanted, uint32_t *value)
{
    int offset = 0;
    while (offset < length)
    {
        int type = options[offset++];
        int option_length;
        if (type == 0)
            continue;
        if (type == DHCP_OPT_END)
            break;
        if (offset >= length)
            return false;
        option_length = options[offset++];
        if (option_length > length - offset)
            return false;
        if (type == wanted && option_length >= 4)
        {
            *value = get_be32(options + offset);
            return true;
        }
        offset += option_length;
    }
    return false;
}

static int option_message(const unsigned char *options, int length)
{
    int offset = 0;
    while (offset < length)
    {
        int type = options[offset++];
        int option_length;
        if (type == 0)
            continue;
        if (type == DHCP_OPT_END)
            break;
        if (offset >= length)
            return 0;
        option_length = options[offset++];
        if (option_length > length - offset)
            return 0;
        if (type == DHCP_OPT_MESSAGE && option_length == 1)
            return options[offset];
        offset += option_length;
    }
    return 0;
}

static void handle_dhcp(const unsigned char *frame, int length)
{
    const unsigned char *ip;
    const unsigned char *udp;
    const unsigned char *bootp;
    const unsigned char *options;
    int ip_length;
    int udp_length;
    int options_length;
    int message;

    if (length < 14 + 20 + 8 + DHCP_FIXED_SIZE ||
        get_be16(frame + 12) != ETH_IPV4)
        return;
    ip = frame + 14;
    ip_length = (ip[0] & 0xf) * 4;
    if ((ip[0] >> 4) != 4 || ip_length < 20 ||
        14 + ip_length + 8 + DHCP_FIXED_SIZE > length || ip[9] != IP_UDP)
        return;
    udp = ip + ip_length;
    udp_length = get_be16(udp + 4);
    if (get_be16(udp) != DHCP_SERVER_PORT ||
        get_be16(udp + 2) != DHCP_CLIENT_PORT || udp_length < 8 +
        DHCP_FIXED_SIZE || 14 + ip_length + udp_length > length)
        return;
    bootp = udp + 8;
    if (bootp[0] != 2 || get_be32(bootp + 4) != network.xid ||
        memcmp(bootp + 28, network.mac, 6) ||
        get_be32(bootp + 236) != DHCP_MAGIC)
        return;
    options = bootp + DHCP_FIXED_SIZE;
    options_length = udp_length - 8 - DHCP_FIXED_SIZE;
    message = option_message(options, options_length);
    if (network.state == DHCP_DISCOVERING && message == DHCP_OFFER)
    {
        network.offered_address = get_be32(bootp + 16);
        if (!network.offered_address ||
            !option_u32(options, options_length, DHCP_OPT_SERVER,
                        &network.server))
            return;
        network.state = DHCP_REQUESTING;
        network.retries = 0;
        network.retry_at = current_tick;
        network.status = "Accepting iPhone hotspot lease";
    }
    else if (network.state == DHCP_REQUESTING && message == DHCP_ACK)
    {
        network.address = get_be32(bootp + 16);
        if (!network.address)
            network.address = network.offered_address;
        option_u32(options, options_length, DHCP_OPT_SUBNET,
                   &network.subnet);
        option_u32(options, options_length, DHCP_OPT_ROUTER,
                   &network.router);
        option_u32(options, options_length, DHCP_OPT_DNS, &network.dns);
        option_u32(options, options_length, DHCP_OPT_LEASE,
                   &network.lease_seconds);
        if (!network.lease_seconds)
            network.lease_seconds = 3600;
        network.lease_at = current_tick;
        network.state = DHCP_BOUND;
        network.status = "iPhone hotspot route ready";
    }
    else if (message == DHCP_NAK)
    {
        network.state = DHCP_FAILED;
        network.status = "iPhone rejected the DHCP lease";
    }
}

static void handle_ipv4(unsigned char *frame, int length)
{
    unsigned char *ip;
    int header_length;
    int total_length;

    if (length < 34 || get_be16(frame + 12) != ETH_IPV4)
        return;
    ip = frame + 14;
    header_length = (ip[0] & 0x0f) * 4;
    total_length = get_be16(ip + 2);
    if ((ip[0] >> 4) != 4 || header_length < 20 ||
        total_length < header_length || 14 + total_length > length)
        return;
    if (ip[9] == IP_UDP && total_length >= header_length + 8)
    {
        unsigned char *udp = ip + header_length;
        int udp_length = get_be16(udp + 4);
        uint16_t source_port = get_be16(udp);
        uint16_t destination_port = get_be16(udp + 2);
        if (udp_length < 8 || udp_length > total_length - header_length)
            return;
        if (source_port == DHCP_SERVER_PORT &&
            destination_port == DHCP_CLIENT_PORT)
        {
            handle_dhcp(frame, length);
            return;
        }
        if (network.state == DHCP_BOUND &&
            get_be32(ip + 16) == network.address)
            queue_udp(destination_port, get_be32(ip + 12), udp + 8,
                      udp_length - 8);
    }
    else if (network.state == DHCP_BOUND && ip[9] == IP_ICMP &&
             total_length >= header_length + 8 &&
             get_be32(ip + 16) == network.address &&
             ip[header_length] == 8)
    {
        unsigned char source_mac[6];
        uint32_t source_ip = get_be32(ip + 12);
        memcpy(source_mac, frame + 6, 6);
        ethernet_header(frame, source_mac, ETH_IPV4);
        put_be32(ip + 12, network.address);
        put_be32(ip + 16, source_ip);
        put_be16(ip + 10, 0);
        put_be16(ip + 10, checksum(ip, header_length));
        ip[header_length] = 0;
        put_be16(ip + header_length + 2, 0);
        put_be16(ip + header_length + 2,
                 checksum(ip + header_length,
                          total_length - header_length));
        usb_iphone_tether_send_frame(frame, 14 + total_length);
    }
}

void usb_iphone_network_reset(void)
{
    memset(&network, 0, sizeof(network));
    udp_read = udp_write = 0;
    network.status = "Off";
}

void usb_iphone_network_service(void)
{
    bool transport = usb_iphone_tether_link_active();
    int length;

    if (!transport)
    {
        if (network.previous_transport)
            usb_iphone_network_reset();
        network.previous_transport = false;
        return;
    }
    if (!network.previous_transport)
    {
        usb_iphone_network_reset();
        network.previous_transport = true;
        if (!usb_iphone_tether_get_mac(network.mac))
        {
            network.status = "Waiting for iPhone Ethernet address";
            return;
        }
        begin_discovery();
    }
    while ((length = usb_iphone_tether_receive_frame(packet,
                                                      sizeof(packet))) > 0)
    {
        if (length >= 14 && get_be16(packet + 12) == ETH_ARP)
            handle_arp(packet, length);
        else
            handle_ipv4(packet, length);
    }

    if ((network.state == DHCP_DISCOVERING ||
         network.state == DHCP_REQUESTING) &&
        !TIME_BEFORE(current_tick, network.retry_at))
    {
        int message = network.state == DHCP_DISCOVERING ?
                      DHCP_DISCOVER : DHCP_REQUEST;
        if (network.retries++ >= DHCP_MAX_RETRIES)
        {
            network.state = DHCP_FAILED;
            network.status = "iPhone hotspot DHCP timed out";
            return;
        }
        send_dhcp(message);
        network.retry_at = current_tick + DHCP_RETRY_TICKS;
    }
    else if (network.state == DHCP_BOUND)
    {
        uint32_t half_lease = MAX(network.lease_seconds / 2, 60u);
        if (TIME_AFTER(current_tick,
                       network.lease_at + (long)(half_lease * HZ)))
            begin_discovery();
    }
}

bool usb_iphone_network_connected(void)
{
    return network.state == DHCP_BOUND && usb_iphone_tether_link_active();
}

uint32_t usb_iphone_network_address(void)
{
    return network.address;
}

uint32_t usb_iphone_network_router(void)
{
    return network.router;
}

uint32_t usb_iphone_network_dns(void)
{
    return network.dns;
}

int usb_iphone_network_udp_send(uint32_t destination, uint16_t source_port,
                                uint16_t destination_port,
                                const void *data, int length)
{
    unsigned char *ip = packet + 14;
    unsigned char *udp = ip + 20;
    uint32_t next_hop;
    int frame_length;

    if (!usb_iphone_network_connected() || !destination || !data ||
        length < 0 || length > UDP_PAYLOAD_MAX)
        return -1;
    next_hop = destination;
    if (network.subnet &&
        (destination & network.subnet) != (network.address & network.subnet))
        next_hop = network.router;
    if (!next_hop)
        return -1;
    if (!network.peer_mac_valid || network.peer_ip != next_hop)
    {
        network.peer_ip = next_hop;
        network.peer_mac_valid = false;
        if (!network.arp_retry_at ||
            !TIME_BEFORE(current_tick, network.arp_retry_at))
        {
            send_arp(next_hop);
            network.arp_retry_at = current_tick + ARP_RETRY_TICKS;
        }
        return 0;
    }
    memset(packet, 0, 14 + 20 + 8);
    ethernet_header(packet, network.peer_mac, ETH_IPV4);
    ip[0] = 0x45;
    put_be16(ip + 2, 20 + 8 + length);
    ip[8] = 64;
    ip[9] = IP_UDP;
    put_be32(ip + 12, network.address);
    put_be32(ip + 16, destination);
    put_be16(ip + 10, checksum(ip, 20));
    put_be16(udp, source_port);
    put_be16(udp + 2, destination_port);
    put_be16(udp + 4, 8 + length);
    memcpy(udp + 8, data, length);
    frame_length = 14 + 20 + 8 + length;
    return usb_iphone_tether_send_frame(packet, frame_length) == frame_length ?
           length : -1;
}

int usb_iphone_network_udp_receive(uint16_t port, uint32_t *source,
                                   void *data, int capacity)
{
    struct udp_message *message;
    int length;
    if (udp_read == udp_write)
        return 0;
    message = &udp_queue[udp_read];
    if (message->port != port)
        return 0;
    if (!data || capacity < message->length)
    {
        udp_read = (udp_read + 1) % UDP_QUEUE_SLOTS;
        return -1;
    }
    length = message->length;
    memcpy(data, message->data, length);
    if (source)
        *source = message->source;
    udp_read = (udp_read + 1) % UDP_QUEUE_SLOTS;
    return length;
}

const char *usb_iphone_network_status(void)
{
    return network.status ? network.status : "Off";
}
