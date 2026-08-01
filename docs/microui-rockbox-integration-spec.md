# microui Rockbox Plugin Integration Specification

## Decision

Port microui as an optional Rockbox plugin library plus a demonstration plugin.
Do not make it a firmware-wide GUI replacement and do not migrate existing
plugins as part of the initial work.

The upstream renderer-independent immediate-mode core is small and portable,
but its default 256 KiB command list, floating-point slider/text parsing, libc
calls, and mouse-oriented input model are not suitable unchanged. The port is
feasible if those concerns remain in a narrow compatibility profile and a
Rockbox adapter.

First qualification target: iPod Classic 6G/7G at 320x240 RGB565, plus the
matching simulator. iPod Video 5G follows after memory, CPU, and click-wheel
navigation measurements pass.

## Upstream and License

Pin the official [microui repository](https://github.com/rxi/microui) at commit
`0850aba860959c3e75fb3e97120ca92957f9d057`.

Import `src/microui.c`, `src/microui.h`, and the MIT license into
`apps/plugins/lib/microui/`. Record the pin, imported files, and every local
compatibility patch in `UPSTREAM.md`. Do not import an SDL renderer or example
assets into firmware.

The upstream core is ANSI C, about 1,100 source lines, uses no dynamic
allocation, and emits renderer commands for clip rectangles, filled
rectangles, text, and icons. It provides windows, panels, buttons, labels,
checkboxes, sliders, text boxes, tree nodes, popups, layout, and word wrapping.

## Repository Shape

```text
apps/plugins/lib/microui/
    microui.c
    microui.h
    microui_config.h
    microui_rockbox.c
    microui_rockbox.h
    microui.make
    LICENSE.upstream
    UPSTREAM.md

apps/plugins/microui_demo/
    SOURCES
    microui_demo.make
    microui_demo.c
```

The upstream pair remains recognizable. `microui_config.h` selects the bounded
Rockbox profile. `microui_rockbox.[ch]` owns drawing, input translation, focus,
keyboard invocation, errors, and lifecycle. Consumers link the plugin library
explicitly; no plugin API version bump or core firmware dependency is allowed.

Package `microui_demo.rock` under `.rockbox/rocks/demos/`. The library itself
does not appear as a launcher item.

## Compatibility Profile

Make upstream constants overrideable with `#ifndef` rather than permanently
changing their public defaults. The initial Rockbox profile is:

| Resource | Rockbox starting limit | Required behavior at limit |
| --- | ---: | --- |
| Command list | 48 KiB | Set an overflow error and stop recording this frame safely. |
| Root list | 8 | Reject the additional root for the frame. |
| Container pool | 16 | Return a visible adapter error, never reuse a live container. |
| Clip/id/layout stacks | 16 each | Abort the frame safely on overflow/underflow. |
| Tree-node pool | 24 | Omit additional nodes and report the diagnostic. |
| Focusable-control registry | 64 | Preserve existing entries; ignore later controls and report overflow. |

These are measurement starting points, not magic ABI constants. A profile may
increase them only if the plugin map and runtime high-water report still leave
comfortable headroom. The release goal is a complete `mu_Context` plus adapter
state below 96 KiB.

Replace the floating default `MU_REAL` with a signed 32-bit integer profile.
Initial sliders and number fields are integral. Provide explicit integer
formatting and a bounded integer parser with overflow detection. Fractional
controls, if later required, use a separately reviewed fixed-point wrapper;
do not silently restore `float`, `strtod`, or libm.

Route formatting through bounded `rb->snprintf` wrappers. Route memory and
string operations through functions available to Rockbox plugins. Replace the
stderr/assert default with `MU_ASSERT` that records file-independent error
codes and safely terminates the current frame in debug builds. A release build
must never continue after a command-buffer or stack write would exceed bounds.

Build gates reject unresolved `malloc`, `free`, `realloc`, `sprintf`, `strtod`,
stdio, libm, SDL, or host operating-system symbols.

## Renderer Adapter

The consumer performs the standard immediate-mode sequence:

1. Feed translated input to `mu_Context`.
2. Call `mu_begin()`.
3. Declare windows and controls.
4. Call `mu_end()`.
5. Iterate `mu_next_command()` and render through the Rockbox adapter.

Translate commands as follows:

| microui command | Rockbox action |
| --- | --- |
| `MU_COMMAND_CLIP` | Intersect with the LCD bounds and current adapter clip rectangle. |
| `MU_COMMAND_RECT` | Convert opaque RGB to native RGB565 and call the clipped fill primitive. |
| `MU_COMMAND_TEXT` | Draw with an already loaded Rockbox font and enforce clip bounds. |
| `MU_COMMAND_ICON` | Draw adapter-owned monochrome/vector glyphs for close, check, collapse, and expand. |
| `MU_COMMAND_JUMP` | Handled by microui command iteration; it emits no pixels. |

The first profile accepts only fully opaque colors. If alpha is requested, the
adapter reports `MU_RB_ERR_ALPHA_UNSUPPORTED` and uses the documented opaque
fallback; it must not read back/blend the entire framebuffer implicitly.

Use `FONT_UI` or `FONT_SYSFIXED` already loaded by Rockbox. Do not call
`font_load()` from the adapter. Implement `text_width` and `text_height` with
the exact font used for drawing so layout and clipping agree.

Track the union of pixels touched during a frame and issue a bounded LCD update
for that dirty region. Fall back to one full-screen update when multiple
disjoint regions or target APIs make the partial update more expensive. Do not
allocate a second full-screen framebuffer, take playback memory, or own display
state beyond the calling plugin's lifetime.

## Click-Wheel Navigation Adapter

microui assumes a pointer, while click-wheel iPods require deterministic focus
navigation. Add a narrow optional hook at the point where microui updates an
interactive control:

```text
MU_CONTROL_HOOK(ctx, id, rect, opt)
```

The default upstream-compatible definition is empty. The Rockbox definition
registers the stable control ID and visible rectangle in a fixed-capacity list
for the current frame. It skips clipped, `MU_OPT_NOINTERACT`, and explicitly
disabled controls. No widget-specific button polling belongs in upstream code.

At the end of a frame, reconcile the selected stable ID with the new registry.
If it disappeared, select the nearest succeeding entry, then the preceding
entry, then the first entry. On the next frame synthesize the pointer position
at the selected rectangle center. This preserves upstream hover/active logic
while giving the wheel a predictable focus model.

Default bindings:

| iPod input | Adapter behavior |
| --- | --- |
| Clockwise / counter-clockwise wheel step | Next / previous focusable control, with acceleration capped and debounced |
| Center press/release | Synthesized primary pointer press/release on the focused control |
| Previous / Next | Decrease / increase a focused slider; horizontal move where the widget explicitly opts in |
| Play/Pause | Activate focused text editing or accept the current dialog |
| Menu | Close top popup/window or report Cancel to the consumer |
| Menu held | Report application exit request |
| Hold switch | Clear all pressed state and suppress activation |

Activating a text box opens `rb->kbd_input()` on the consumer's bounded backing
buffer. On return, reconcile focus and invalidate layout; do not emulate typed
characters through wheel events. The adapter also retains normal pointer APIs
so future touch targets can feed real coordinates without using the focus
synthesizer.

## Consumer API

Expose a small adapter API, independent of `plugin_start()`:

```c
enum mu_rb_status mu_rb_init(struct mu_rb *ui, mu_Context *ctx,
                             const struct mu_rb_config *config);
enum mu_rb_status mu_rb_poll(struct mu_rb *ui);
enum mu_rb_status mu_rb_begin(struct mu_rb *ui);
enum mu_rb_status mu_rb_render(struct mu_rb *ui);
void mu_rb_shutdown(struct mu_rb *ui);
```

Also expose focus-next/focus-previous, open-keyboard, error query, clear-error,
and high-water diagnostic helpers. All storage is supplied by the consumer or
embedded in fixed structs. Initialization validates sizes and target
capabilities before modifying LCD state.

The consumer owns its model and string buffers. microui/adapter state contains
no filesystem path, persistent configuration, thread, timer callback, or
global singleton. Multiple contexts may exist sequentially; simultaneous
contexts are supported only if each has independent storage and only one draws
at a time.

## Demo and Acceptance Surface

`microui_demo.rock` is both an example and a regression surface. It contains:

- A movable/scrollable window and nested panel.
- Labels, wrapped text, buttons, checkbox, integer slider, text box, tree node,
  popup, and modal confirmation.
- Controls that appear/disappear to verify stable-ID focus recovery.
- A dense stress page that approaches command, container, stack, and focus
  limits without crossing memory guards.
- A diagnostics page showing context size, command bytes, roots, containers,
  controls, dirty rectangle, frame time, and the last adapter error.

The demo uses only generated colors/text/icons; no external assets are needed.
It must exit through one cleanup path, clear input state, restore LCD colors,
viewport, font selection, and backlight policy, and return
`PLUGIN_USB_CONNECTED` after a USB event.

## Audio and Memory Isolation

microui and its demo have no audio function. They must not call
`plugin_get_audio_buffer()`, `audio_stop()`, mixer APIs, playlist APIs, or
take/release callbacks. UI memory comes from the consumer's normal plugin
region or `plugin_get_buffer()` and is bounded before initialization.

This rule also applies to future consumers: linking microui does not authorize
a decorative UI to claim playback memory. Any consumer that separately owns
audio must follow `docs/plugin-audio-lifecycle-steering.md` and keep its audio
lifetime independent of the microui context.

## Testing and Verification

Host/core tests cover command serialization, clip intersections, layout,
stable IDs, bounded integer parsing, format truncation, every configured
overflow, and malformed push/pop sequences. Guard bytes surround the command
list, pools, focus registry, and consumer text buffers.

Simulator tests use scripted wheel/button events and framebuffer captures to
verify:

- Each widget receives focus and activation in declared order.
- Dynamic removal and popup/modal transitions retain valid focus.
- Text keyboard cancel and accept preserve bounds and redraw correctly.
- Scrolling cannot draw outside nested clips.
- Command/pool overflow reports an error without corruption or an infinite
  loop.
- Hold and USB clear active/hover/pressed state.
- Dirty-region and full-update paths render identical final pixels.

Hardware qualification records average/95th/worst build-and-render time for a
normal page and the stress page. The UI must remain responsive while music is
playing in the background, and entering/exiting the demo from Database and
Files must not alter playlist, playback, or audio-buffer state.

Compile/link verification confirms no heap, float/libm, stdio, SDL, dynamic
font, or shared-audio-buffer dependency. Record `sizeof(mu_Context)`, adapter
size, plugin image size, stack high-water, and runtime pool high-water for both
simulator and hardware builds.

## Delivery Milestones

1. **Core gate:** pinned MIT import, overrideable limits, integer profile,
   bounded errors, host tests, and symbol audit.
2. **Renderer gate:** RGB565 clip/rect/text/icon rendering and golden simulator
   captures with no secondary framebuffer.
3. **Navigation gate:** stable-ID focus registry, click-wheel activation,
   keyboard bridge, hold, Menu, and USB behavior.
4. **Demo gate:** all widgets, stress/diagnostic pages, memory high-water
   report, and physical iPod 6G interaction test.
5. **Library gate:** documented consumer API, makefile integration, example,
   and one small opt-in proof consumer if useful.
6. **5G gate:** profile and qualify the same bounded library configuration on
   iPod Video without weakening memory or responsiveness criteria.

Existing plugin migration is a separate proposal. The library is ready when
the demo and adapter are safe and reproducible, not when every Rockbox menu has
been rewritten.
