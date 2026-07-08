# Custom OS Visual Plan

## Purpose

ZeroSlackr is now the working shell/runtime base.
iPodLinux is the experimental boot and userland base.
Rockbox remains the reference for solved hardware behavior.
The Rockbox `iClassic` theme is the first Apple Classic visual reference.

The long-term goal is not "make Podzilla prettier."
The long-term goal is a new custom iPod OS that looks and behaves like Apple's 6G/7G Classic firmware while running on 5G hardware.

## Layer Model

### Layer 0: Boot / Safety

- Apple firmware remains recoverable
- Rockbox remains bootable
- ZeroSlackr/iPodLinux remains experimental
- future custom OS replaces the experimental shell only after it is stable

### Layer 1: Hardware / Runtime Base

Use iPodLinux / ZeroSlackr for:

- early boot experiments
- framebuffer output
- shell iteration
- input testing
- safe userland prototyping

Use Rockbox for reference on:

- audio pipeline
- codec handling
- storage behavior
- power behavior
- clickwheel/button semantics
- mature theme/rendering decisions

### Layer 2: Visual Shell

The first serious visual target is the Apple Classic 6G/7G look:

- top status/header bar
- left-side menu list
- right-side preview panel
- bright white / light gray base
- blue selection gradient
- dark text
- clean separators
- compact Apple-style proportions

The Rockbox `iClassic` theme is the first-pass visual art source, not the final implementation architecture.

### Layer 3: Future Custom OS

After the visual shell is proven, the next later milestones can cover:

- real library integration
- playback state integration
- album art pipeline
- transitions / animation
- deeper settings
- plugin/app launcher model

## This Milestone

This milestone is intentionally narrow.

Success means:

- BOOT
- DRAW APPLE-STYLE MAIN MENU
- NAVIGATE MENU SAFELY
- EXIT / RETURN SAFELY

Not in scope yet:

- Cover Flow
- playback rewrite
- database rewrite
- boot splash replacement
- firmware flashing
- permanent boot changes

## Why A Mockup Path Exists First

The current repo contains ZeroLauncher build scripts and patch files, but not a full local upstream `podzilla2` source checkout.

That means:

- assets, schemes, fonts, configs can be staged immediately
- visual mockups can be generated immediately
- a true rebuilt on-device Apple-style shell needs a later source-rebuild pass

This is the correct order for a conservative boot/runtime project:

1. stage assets
2. define the target shell
3. generate screenshots
4. validate look and navigation model
5. only then modify the live runtime implementation
