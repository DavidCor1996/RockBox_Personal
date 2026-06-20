# Nano 3G Local wInd3x Audit

Date: 2026-04-25  
Repo under audit: `/tmp/wInd3x`  
Baseline commit: `4d2a4f1a73f98269484775f867c5ec8be3b9bd36`

## Purpose

Freeze the current local Nano 3G `wInd3x` state against the clean cloned
baseline before any further hardware tests.

## Executive Summary

The persistent tracked code drift in local `wInd3x` is small in file count but
large in behavioral impact:

- modified tracked files:
  - `pkg/cfw/defang_wtf.go`
  - `pkg/image/image.go`
- untracked Nano 3G helper patch tools:
  - `52` files matching `cmd/patch_n3g_*.go`

So the current Nano 3G results are **not** results from stock upstream
`wInd3x`. They are results from a local experimental branch layered on top of
upstream.

## Tracked Diff

### `pkg/image/image.go`

This is the small, well-scoped change:

- Nano 3G no longer forces IMG1 format `2`
- Nano 3G now uses the standard X509-style unsigned wrapper layout:
  - format `4`
  - signature area `0x80`
  - certificate area `0x300`

This change was directly motivated by recovered BootROM behavior:

- callback `0x200006dc`
- handoff mode `r2 = 2`
- accepts only IMG1 type:
  - `3`
  - `4`

### `pkg/cfw/defang_wtf.go`

This is the large, high-risk drift.

The upstream-like Nano 3G state originally centered on a minimal raw WTF
defanger. The current local file is now a broad staged patch stack that mixes:

- restored original WTF control-flow edges
- targeted branch/callsite replacement
- local readiness stub
- local loader callback stub
- local UART immediate-return bypass
- multiple marker/probe stubs placed into free WTF body space
- DRAM execution probes

Representative active offsets in the current local file include:

- control-flow restoration / shaping:
  - `0x1758`
  - `0x1768`
  - `0x177c`
  - `0x1788`
  - `0x1798`
  - `0x17a0`
  - `0x181c`
  - `0x182c`
  - `0x1938`
  - `0x193c`
  - `0x197c`
  - `0x1980`
  - `0x1988`
  - `0x1990`
  - `0x1994`
  - `0x1998`
  - `0x19b0`
  - `0x19b8`
  - `0x19c8`
  - `0x19d0`
  - `0x19d4`
  - `0x19d8`
  - `0x19dc`
  - `0x1a24`
  - `0x1a28`
  - `0x1a4c`
  - `0x1a78`
  - `0x1ab0`
  - `0x1ae0`
  - `0x1ae8`
  - `0x1b04`
  - `0x1b0c`
  - `0x1b14`
  - `0x24b8`
  - `0x24c8`
- active local stubs / bypasses:
  - `0x6ccc`
  - `0x6e00`
  - `0x6558`
- active or prepared marker/probe ranges:
  - `0x6f00..0x6fe0`
  - `0x76d0`
  - `0x76d4`
  - `0x76e8`
  - `0x7708`
  - `0x7758`

This is no longer a single hypothesis. It is an accumulated test harness.

## Untracked Helper Tools

The local `/tmp/wInd3x/cmd` directory currently contains `52` Nano 3G-specific
helper programs named like:

- `patch_n3g_connected_ui.go`
- `patch_n3g_later_beep_signal.go`
- `patch_n3g_runtime_signal.go`
- `patch_n3g_scheduler_probe.go`
- `patch_n3g_display_hw_init.go`
- `patch_n3g_direct_payload_08000800.go`
- `patch_n3g_usb_drop_2f98_loop.go`
- many additional staged marker/bypass helpers

These helpers are not part of the tracked diff, but they are evidence that the
current Nano 3G workflow in `/tmp/wInd3x` has become a local lab branch rather
than a narrowly-scoped fix set.

## Interpretation

### What is clean enough to keep

- the Nano 3G IMG1 wrapper fix in `pkg/image/image.go`

This is still the strongest evidence-backed persistent change.

### What is no longer clean enough to trust as a baseline

- the current Nano 3G `pkg/cfw/defang_wtf.go`

It has too many intertwined staged edits to support clear causal reasoning from
a single hardware result.

### MacPod / WinPod relevance

Discovering that the device is a **MacPod** instead of a **WinPod** does **not**
explain the current failures:

- the failures occur in:
  - `BootROM -> WTF -> RetailOS` handoff
- they occur before:
  - filesystem mounting
  - FAT32/HFS+ handling
  - normal mass-storage/UI behavior

MacPod vs WinPod can still matter later for:

- restore/repartition flows
- disk mode behavior after successful boot
- FAT32 vs HFS+ installation/update assumptions

But it is not the current blocker.

## Recommended Freeze Boundary

Before more hardware tests, treat the current local Nano 3G `wInd3x` state as:

- **experimental branch frozen for audit**

Recommended next cleanup step before further runs:

1. preserve the `pkg/image/image.go` format-4 wrapper fix
2. reset Nano 3G `pkg/cfw/defang_wtf.go` to one intentionally small test state
3. keep all other staged helpers out-of-band for reference only

That would restore a testable cause/effect relationship for future hardware
results.

## Current Audit Decision

- **LOCAL_PATCH_STATE_FROZEN_FOR_AUDIT**
