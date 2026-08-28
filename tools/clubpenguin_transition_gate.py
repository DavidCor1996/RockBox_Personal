#!/usr/bin/env python3
"""Validate Club Penguin room bounds and automatic doorway transitions."""

from __future__ import annotations

import argparse
import csv
from dataclasses import dataclass
from pathlib import Path


PLAYER_HALF_W = 26
PLAYER_HALF_H = 28
DOOR_BODY_HALF_W = 12
DOOR_BODY_HALF_H = 16
DOOR_MAX_RADIUS = 18


@dataclass(frozen=True)
class Room:
    room_id: str
    start_x: int
    start_y: int
    left: int
    top: int
    right: int
    bottom: int


@dataclass(frozen=True)
class Door:
    room_id: str
    door_id: str
    x: int
    y: int
    radius: int
    target: str


def rows(path: Path) -> list[list[str]]:
    with path.open(newline="", encoding="utf-8") as source:
        return [row for row in csv.reader(source, delimiter="\t")
                if row and not row[0].startswith("#")]


def load_rooms(path: Path) -> dict[str, Room]:
    result: dict[str, Room] = {}
    for row in rows(path):
        result[row[0]] = Room(
            row[0], int(row[3]), int(row[4]), int(row[5]), int(row[6]),
            int(row[7]), int(row[8]),
        )
    return result


def load_doors(path: Path) -> dict[str, list[Door]]:
    result: dict[str, list[Door]] = {}
    for row in rows(path):
        if row[5] != "room":
            continue
        result.setdefault(row[0], []).append(
            Door(row[0], row[1], int(row[2]), int(row[3]), int(row[4]),
                 row[6])
        )
    return result


def clamp(room: Room, x: int, y: int) -> tuple[int, int]:
    left = max(PLAYER_HALF_W, room.left)
    top = max(PLAYER_HALF_H, room.top)
    right = min(320 - PLAYER_HALF_W, room.right)
    bottom = min(220 - PLAYER_HALF_H, room.bottom)
    return min(max(x, left), right), min(max(y, top), bottom)


def inside(door: Door, x: int, y: int) -> bool:
    closest_x = min(max(door.x, x - DOOR_BODY_HALF_W),
                    x + DOOR_BODY_HALF_W)
    closest_y = min(max(door.y, y - DOOR_BODY_HALF_H),
                    y + DOOR_BODY_HALF_H)
    dx = door.x - closest_x
    dy = door.y - closest_y
    radius = min(door.radius, DOOR_MAX_RADIUS)
    return dx * dx + dy * dy <= radius * radius


def matching_arrival(rooms: dict[str, Room], doors: dict[str, list[Door]],
                     source: str, target: str) -> tuple[int, int]:
    room = rooms[target]
    for door in doors.get(target, []):
        if door.target != source:
            continue
        dx = room.start_x - door.x
        dy = room.start_y - door.y
        radius = min(door.radius, DOOR_MAX_RADIUS)
        if abs(dx) > abs(dy):
            clearance = radius + DOOR_BODY_HALF_W + 1
            x = door.x + (-clearance if dx < 0 else clearance)
            y = door.y
        else:
            clearance = radius + DOOR_BODY_HALF_H + 1
            x = door.x
            y = door.y + (-clearance if dy < 0 else clearance)
        return clamp(room, x, y)
    return clamp(room, room.start_x, room.start_y)


def walk_to(rooms: dict[str, Room], doors: dict[str, list[Door]],
            room_id: str, x: int, y: int, target_x: int,
            target_y: int) -> tuple[str, int, int, int]:
    room = rooms[room_id]
    x, y = clamp(room, x, y)
    armed = not any(inside(door, x, y) for door in doors.get(room_id, []))
    travelled = 0
    while (x, y) != (target_x, target_y) and travelled < 1000:
        if x != target_x:
            x += 2 if x < target_x else -2
        elif y != target_y:
            y += 2 if y < target_y else -2
        x, y = clamp(room, x, y)
        travelled += 2
        contacts = [door for door in doors.get(room_id, [])
                    if inside(door, x, y)]
        if contacts:
            if armed:
                door = min(contacts,
                           key=lambda item: (x - item.x) ** 2 +
                           (y - item.y) ** 2)
                x, y = matching_arrival(rooms, doors, room_id, door.target)
                return door.target, x, y, travelled
        else:
            armed = True
    return room_id, x, y, travelled


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--assets", type=Path, required=True)
    args = parser.parse_args()
    data = args.assets / "data"
    rooms = load_rooms(data / "rooms.tsv")
    doors = load_doors(data / "interactions.tsv")

    for room_id, room_doors in doors.items():
        for door in room_doors:
            assert door.target in rooms, (room_id, door.door_id, door.target)

    checks = [
        ("Tom's Diner exit", "coffee_shop", 160, 170, 154, 90, "town"),
        ("Tom's Diner stairs", "coffee_shop", 160, 170, 286, 105,
         "book_room"),
        ("Arena straight entry", "plaza", 160, 170, 160, 112, "stage"),
        ("Sewer Sessions manhole", "plaza", 160, 170, 116, 150,
         "emma_sewer"),
        ("Sewer Sessions exit", "emma_sewer", 160, 180, 26, 100,
         "plaza"),
        ("Buck-A-Slice entry", "plaza", 160, 170, 242, 112,
         "pizza_parlor"),
        ("Clock District arena door", "snow_forts", 160, 170, 160, 103,
         "stadium"),
        ("Clock District Velvet Ice door", "snow_forts", 160, 170,
         92, 100, "velvet_ice"),
        ("Velvet Ice exit", "velvet_ice", 160, 170, 160, 82,
         "snow_forts"),
        ("Clock District Chrome Clinic door", "snow_forts", 160, 170,
         228, 100, "chrome_clinic"),
        ("Chrome Clinic exit", "chrome_clinic", 160, 170, 160, 82,
         "snow_forts"),
        ("Arasaka Headquarters entry", "dojo_courtyard", 160, 170,
         160, 100, "dojo"),
        ("Arasaka lobby exit", "dojo", 160, 170, 160, 82,
         "dojo_courtyard"),
        ("Memorial Park west path", "forest", 160, 170, 36, 120,
         "plaza"),
        ("Memorial Park east path", "forest", 160, 170, 284, 116,
         "cove"),
        ("Memorial Park conservatory", "forest", 160, 170, 112, 104,
         "puffle_wild"),
        ("Memorial Park NCART underpass", "forest", 160, 170,
         224, 104, "hidden_lake"),
        ("Clock District left street", "snow_forts", 160, 170, 30, 126,
         "town"),
        ("Clock District right street", "snow_forts", 160, 170,
         290, 126, "plaza"),
        ("Laguna cottage entry", "dock", 160, 170, 164, 105,
         "lighthouse"),
        ("Cottage rooftop stairs", "lighthouse", 160, 170, 286, 103,
         "beacon"),
        ("Rooftop hatch", "beacon", 160, 170, 166, 125,
         "lighthouse"),
        ("Shore cottage entry", "beach", 160, 170, 176, 105,
         "lighthouse"),
    ]

    fort = rooms["snow_forts"]
    fort_x, fort_y = clamp(fort, fort.start_x, fort.start_y)
    assert not any(inside(door, fort_x, fort_y)
                   for door in doors["snow_forts"])
    print("PASS Snow Forts spawn is outside every automatic door")

    for label, room_id, x, y, target_x, target_y, expected in checks:
        actual, arrival_x, arrival_y, travelled = walk_to(
            rooms, doors, room_id, x, y, target_x, target_y
        )
        assert actual == expected, (label, actual, expected)
        assert not any(inside(door, arrival_x, arrival_y)
                       for door in doors.get(actual, [])), (
                           label, actual, arrival_x, arrival_y)
        print(f"PASS {label}: {room_id} -> {actual} after {travelled}px; "
              f"safe arrival=({arrival_x},{arrival_y})")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
