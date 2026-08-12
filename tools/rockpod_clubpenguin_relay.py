#!/usr/bin/env python3
"""Tiny two-user UDP relay for RockPod Club Penguin rooms."""

import argparse
import socket
import time


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--bind", default="0.0.0.0")
    parser.add_argument("--port", type=int, default=47701)
    args = parser.parse_args()
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.bind((args.bind, args.port))
    rooms: dict[bytes, dict[tuple[str, int], float]] = {}
    print(f"Club Penguin two-user relay listening on {args.bind}:{args.port}")
    while True:
        packet, address = sock.recvfrom(2048)
        if len(packet) < 24 or not packet.startswith(b"CPR1"):
            continue
        room = packet[4:20]
        now = time.monotonic()
        peers = rooms.setdefault(room, {})
        for peer, seen in list(peers.items()):
            if now - seen > 15:
                del peers[peer]
        if address not in peers and len(peers) >= 2:
            continue
        peers[address] = now
        for peer in peers:
            if peer != address:
                sock.sendto(packet, peer)


if __name__ == "__main__":
    raise SystemExit(main())
