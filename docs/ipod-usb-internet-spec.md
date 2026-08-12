# iPod USB Internet mode

## User contract

`USB Connection` appears in iPodJS Quick Settings with `Storage`, `Charge
Only`, and `Internet`. A mode change applies on the next cable connection.
Internet mode keeps the Rockbox UI, plugins, storage, and playback alive; it
does not export the disk. Once the PC selects the Ethernet data interface, the
official Apple Wi-Fi status glyph occupies the Bluetooth slot. Bluetooth is
suppressed in that slot while Internet is active.

The first supported services are live Weather synchronization, the Safari
browser, and optional two-player Club Penguin. Weather refreshes when the link
comes up and every 15 minutes. In Club Penguin, hold Select to enable or
disable multiplayer. A room contains at most one remote player.

## USB and network protocol

The iPod exposes a standards-based USB CDC Ethernet Control Model function
with a communication interface, CDC Ethernet functional descriptor, interrupt
notification endpoint, and alternate-setting data interface with bulk IN and
OUT endpoints. Linux binds this to `cdc_ether` without a custom kernel driver.

The point-to-point subnet is fixed to avoid DHCP and DNS memory overhead:

- PC: `10.77.0.1/24`, locally administered MAC assigned by the host
- iPod: `10.77.0.2/24`, MAC `02:00:00:00:00:02`
- MTU: 1500 bytes

The fixed-memory iPod network path implements Ethernet II, ARP, IPv4, ICMP
echo, and UDP. It intentionally does not put HTTP, TLS, DNS, or a general TCP
socket stack into firmware. The PC companion terminates HTTPS and converts
Internet responses into bounded application datagrams.

Weather uses UDP 47700 and `RPI1` messages: begin (size and optional CRC),
sequential offset-tagged data chunks, acknowledgements with retry, commit, and
refresh request. Rockbox caps
the file at 32 KiB, writes `forecast.usb.tmp`, and atomically renames it to
`forecast.tsv` only after a complete transfer.

Club Penguin uses UDP 47701. `CPM1` state packets carry a random session ID,
room ID, position, facing, and animation frame at 10 Hz. The PC wraps these in
`CPR1` with a 16-byte pair code. The reference relay admits no more than two
active endpoints per code and expires them after 15 seconds.

The browser uses UDP 47702 and `RPB1` request, begin, sequential data,
acknowledgement, commit, and error messages. The response is a bounded `RPWB`
bundle containing compact semantic HTML plus a 310x170 BMP rendering of the
real site. The companion loads HTTPS, CSS, and JavaScript in Qt WebEngine
(Chromium), captures the finished iPod-sized viewport, and sends it with an
absolute-link list. Rockbox caps a bundle at 256 KiB and splits it into
`live.html` and `live.bmp` only after a complete acknowledged transfer. The
iPod does not execute untrusted JavaScript or parse TLS.

The iPodJS Main Menu exposes `Internet` only when Internet mode has an active
CDC-ECM data link. It launches the real `https://www.google.com/` page. Search
and address entry call the same stock click-wheel keyboard implementation and
Apple keyboard surfaces as Music Search.

RockPod's Video Out screen uses UDP 47704 and `RPF1`. The host requests one
frame at a time; a background-priority worker on the iPod replies with bounded
RGB565 packets. Up to ten ordinary Ethernet/UDP datagrams are aggregated into
one 16 KiB CDC-NCM transfer. This keeps standards-based host networking while
removing most USB completion round trips. Both ordinary UI and active YUV video
remain native 320x240. UI packets may use per-packet PackBits compression;
video stays unmodified RGB565 so motion never becomes blocky from a lossy or
reduced-resolution fallback. The transmit worker submits the next batch only
after the USB completion interrupt releases its bounded semaphore. That chains
full-frame batches without the former scheduler-tick polling delay or an
unbounded one-transfer-per-datagram burst. Native framebuffer strips are copied
one contiguous row span at a time rather than through a per-pixel address loop.
The request carries the last completed 16-bit LCD generation; an unchanged
screen receives one small status packet instead of another full framebuffer.
`lcd_blit_yuv()` mirrors its already-converted pixels into the existing
Rockbox framebuffer only while external capture is active, so MPEGPlayer and
OpenH264 do not appear black. No additional full-screen buffer or playback
memory is allocated.

The request's optional flag byte selects external-only presentation. RockPod
sets it by default: the iPod LCD presents a static, Classic-style dock cable
page reading `TV Out Enabled` while ordinary UI and video pixels continue only
to the external framebuffer stream. The page is rendered into the LCD
driver's existing DMA staging buffer, independently of `FBADDR`; it does not
allocate, copy, or retain a decorative full-screen framebuffer. Normal local
presentation is restored on Stop or link loss. Small remote-action packets
become ordinary button queue events and are ignored while Hold is engaged.
In Internet mode the same USB configuration also exposes Rockbox's existing
stereo 16-bit USB Audio Class source. It pulls the final PCM mixer output and
RockPod monitors that source to the laptop's default speakers, making the PC
serve as the digital stand-in for the portable dock's display and audio path.
Outside Internet mode USB Audio retains its accessory-only configuration.

## PC usage

On Linux, select Internet in Quick Settings, reconnect the cable, then open
RockPod's Video Out screen for the live iPod display, or run the standalone
companion for weather and browser services:

```sh
sudo tools/rockpod_usb_internet.py --location Moncton \
  --latitude 46.0878 --longitude -64.7782
```

The live browser additionally requires `qmlscene6`, Qt WebEngine, and Pillow
on the companion PC. Google and other modern pages are rendered by the real
Chromium engine, not by a locally drawn imitation.

For remote two-player, run the relay on an Internet-reachable PC:

```sh
tools/rockpod_clubpenguin_relay.py --port 47701
```

Each player's companion uses the same private pair code:

```sh
sudo tools/rockpod_usb_internet.py --relay relay.example:47701 --room igloo42
```

## Fixed memory and lifecycle

USB Ethernet uses fixed 16 KiB CDC-NCM receive/transmit buffers, a bounded UDP
transmit buffer and mailbox, and a ten-packet (14 KiB) framebuffer batch. These
are network transport buffers, not full-screen decoration buffers. No buffer
comes from `core_alloc`, playback memory, or the plugin audio buffer. USB
completion work performs no filesystem I/O. Weather file I/O runs only from
the bounded application service point. Status drawing reads one scalar link
flag and a preloaded 22x16 bitmap; it performs no I/O, decode, allocation, or
USB work.

The USB Audio source is the existing PCM pull-mode class driver. It advertises
only one isochronous input endpoint and uses two fixed 192-byte DMA buffers; it
does not allocate its obsolete multi-megabyte ring or sink buffers from shared
playback memory. The route becomes active only when the laptop opens its
capture endpoint. While active it inhibits target I2S DMA and the physical
headphone/line output; stopping RockPod's viewer or disconnecting USB closes
the source and returns audio ownership through the normal Rockbox lifecycle.

## iPhone transport boundary

A direct iPhone tether is technically possible only when the client becomes
the USB host, as supported Android devices do. The iPhone remains the USB
device and exposes Apple's USB Ethernet protocol. The current CDC-ECM design
instead makes the iPod a USB device and the PC the host, so it cannot use that
path yet. Rockbox has no production S5L8702 USB-host/iPhone `ipheth` stack, and
the 30-pin port is not currently being driven as a powered host. A passive
cable alone does not change those controller and VBUS roles.

An iPhone can still be the upstream Internet source when a PC sits between
the devices: the PC joins the iPhone Personal Hotspot and runs the RockPod
companion on the separate CDC-ECM link to the iPod. A direct implementation
would require qualifying 30-pin VBUS sourcing (or using an externally powered
adapter), adding a DesignWare USB host-controller stack, enumerating the
iPhone, handling Trust/pairing as required, and porting the `ipheth` Ethernet
protocol. It is a separate host-mode driver project and is not represented as
working by the current firmware.

## Direct iPhone tether architecture

Direct tethering is a separate USB mode named **iPhone Tether**. It must never
be selected automatically from an ordinary VBUS insertion because the iPod
normally operates as a USB peripheral and its dock power pin is an input. The
mode is available only on the iPod 6G target and leaves Mass Storage, Charge,
and PC Internet behavior unchanged.

The supported physical topology is:

```
iPod 30-pin D+/D- -- powered USB adapter or hub -- iPhone USB
                              |
                         regulated 5 V VBUS
```

The adapter supplies VBUS to the iPhone. Firmware forces the S5L8702
DesignWare OTG controller into host mode but does not enable an unverified
board-level boost or power switch. A passive 30-pin-to-USB-C cable is not a
supported power topology unless electrical testing proves that it provides
regulated VBUS without back-powering the iPod.

### Host state machine

The host transport is bounded and single-device. It has no hub class and no
general-purpose USB API. Its states are Off, Waiting for port, Resetting,
Addressing, Reading descriptors, Selecting tether interface, Waiting for
trust, Link, and Error. Each hardware wait has a deadline; disconnect or mode
change returns to Off and restores the peripheral controller path.

Enumeration accepts Apple vendor `0x05ac` and locates an interface by class,
subclass, and protocol `ff/fd/01`, rather than relying on a product ID. It
selects the alternate setting containing one bulk IN and one bulk OUT
endpoint. The implementation uses Apple's vendor requests documented by the
upstream Linux `ipheth` driver:

- request `0x00`, device-to-host vendor, returns the Ethernet MAC;
- request `0x45`, device-to-host vendor, reports carrier state;
- request `0x04`, host-to-device vendor, optionally enables NCM receive
  framing.

Legacy receive mode begins with two alignment bytes followed by one Ethernet
frame. NCM mode is optional: transmit remains a raw Ethernet frame while
receive uses the fixed iOS NTH16/NDP16 layout. Buffers are fixed, DMA aligned,
and owned by the USB host driver; no audio or core allocator memory is used.
The initial legacy transport reserves approximately 4.3 KiB of static
workspace: a 1 KiB descriptor/control buffer, a 1,602-byte DMA transfer
buffer, a 1,600-byte completed-frame buffer, and small controller state. DHCP
qualification adds one 600-byte packet buffer and small lease state, keeping
the complete direct-tether workspace below 5 KiB.

### Trust and pairing

Carrier is not considered usable until the iPhone has trusted the RockPod
host identity. A production implementation needs usbmux framing, lockdownd
pairing records, certificate/private-key storage, the on-phone Trust flow, and
the TLS version required by the connected iOS release. Pairing data belongs in
`/.rockbox/rockpod/iphone/`, is never logged, and must be written atomically.
Importing a pairing identity is allowed only as an explicit diagnostic path;
enumerating an interface or reading its MAC is not reported as Internet
connectivity.

### Network layer

After carrier, the iPod obtains IPv4 configuration using DHCP over the ipheth
Ethernet frames. The client accepts only replies matching its transaction ID
and hardware address, validates all option lengths, and records address,
subnet, router, lease, and DNS. Disconnect, carrier loss, or lease expiry
removes the route immediately. ARP, ICMP, UDP, DNS, TCP, HTTP, and TLS use the
same bounded packet service; the existing PC companion protocol remains a
different backend.

The Safari-style browser must not appear merely because USB enumeration
succeeded. It appears after a usable IP route and after its content backend is
available. The current PC mode renders pages in the Qt companion. Direct
iPhone mode therefore also requires an on-device HTTP/TLS/rendering backend or
an explicitly configured remote rendering service before it can claim to load
Google. iPhone Personal Hotspot does not itself provide that renderer.

### Qualification gates

The feature is not hardware-qualified until all of these pass:

1. powered-adapter VBUS voltage and current direction are measured before the
   iPhone is attached;
2. connect/disconnect loops restore Mass Storage and PC Internet modes;
3. device, configuration, interface, and endpoint descriptors are captured;
4. the Trust prompt completes and survives reconnect without exposing keys;
5. carrier, DHCP renewal, ARP, DNS, and an HTTPS request pass independently;
6. browser visibility tracks the usable route rather than cable presence;
7. music playback and database navigation continue without buffer shrink,
   restart, or storage unmount;
8. invalid descriptors, NAK storms, stalls, and sudden unplug all hit bounded
   recovery paths rather than hangs.

### Implementation status

Implemented in this tree:

- the target-scoped **iPhone Tether** USB setting and safe role restoration;
- forced S5L8702 DesignWare host mode without claiming a VBUS source;
- bounded port reset, address assignment, configuration scanning, and Apple
  vendor/interface validation;
- alternate-interface and bulk-endpoint discovery;
- legacy two-byte-aligned ipheth receive frames, raw transmit frames, MAC and
  carrier requests, fixed DMA buffers, and disconnect/error deadlines;
- a bounded DHCP client that validates transaction ID, hardware address,
  packet lengths, and options before accepting an IPv4 lease;
- Trust, Link, and Error notifications; and
- connection gating that deliberately does not expose Internet merely from
  enumeration or carrier.

Not yet represented as complete or hardware-qualified:

- usbmux/lockdownd host pairing and secure pairing-record persistence;
- ARP, DNS, TCP, TLS, and a general socket API above the qualified DHCP route;
- DNS/TCP/TLS and a direct browser content backend; and
- electrical qualification of the powered 30-pin adapter topology.

The firmware slice is therefore an enumeration and ipheth transport bring-up,
not a claim that a passive cable can already load Google. Hardware work must
proceed in the order listed by the qualification gates.

Protocol references:

- Linux `ipheth`: <https://github.com/torvalds/linux/blob/master/drivers/net/usb/ipheth.c>
- usbmuxd device transport: <https://github.com/libimobiledevice/usbmuxd>
- lockdownd pairing: <https://github.com/libimobiledevice/libimobiledevice>
