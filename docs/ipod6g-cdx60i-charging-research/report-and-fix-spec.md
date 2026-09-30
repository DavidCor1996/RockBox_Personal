# iPod Classic charging on the TEAC CD-X60i

## Findings and recommended decision

The leading software explanation is an insufficient USB input-current allowance, compounded by custom charger detection that depends on the backlight. The current Classic target defaults to `USB_CHARGING_ENABLE` rather than `USB_CHARGING_FORCE`. In this tree, an unconfigured USB connection normally receives a 100 mA allowance; Force permits 500 mA after the USB core declares no host. The target also disables battery charging through GPIO C1 when it considers the source insufficient. These are concrete reachable code paths, not proof of the electrical state of the particular dock.[^4][^5]

There is also a model-specific operating condition: TEAC says the CD-X60i charges the docked iPod while the system is on and does not charge it in standby. A sleep-timer transition can therefore remove charging without physically undocking the player. The cited manual is the Japanese-market edition; the label/manual of a different regional revision takes precedence.[^1]

The recommended sequence is to measure the powered-on dock with the existing On and Force settings, identify whether it supplies the USB or dedicated adapter path, and then implement a single-owner charging policy. Do not blindly switch the status input to another pin, raise the undocumented doubled-current setting, or treat a successful video handshake as evidence of available charging current.

**Status:** research and implementation specification. No firmware fix has been implemented or hardware-qualified. The same-device Apple-firmware charging comparison, actual installed firmware identity, dock supply classification, and measured battery-current direction remain unknown. The code baseline inspected is the personal working tree at HEAD `04984f554e19c722e2d6c7f6ae7919ecd8072f8c`, with pre-existing modifications; it is not an assertion about the binary currently running on the iPod.

## Hardware and Apple firmware evidence

### What the manufacturer documents establish

Apple's Classic guide describes charging through a powered computer USB connection or Apple USB power adapter, distinguishes charging and full icons, and says playback, video and file transfers can lengthen charging. It recommends a high-power USB port. This establishes the intended consumer behavior; it does not publish GPIO settings, current-source classification thresholds, or the accessory handshake for this TEAC dock. The 120 GB Classic guide is a related Classic-generation reference, not an exact firmware-version trace for every 2007–2009 board.[^2]

The LTC4066 datasheet distinguishes input current, battery charging and battery supplementation of the system. HPWR selects the input-current range. USB suspend restricts the USB path; externally powering OUT can still support charging. POL indicates battery-current direction; CHRG is a charge-cycle/status output, can deassert before all charging ends, and can retain its previous state during an NTC fault. They are not interchangeable. Open-drain outputs also require valid biasing before a high-impedance state becomes a meaningful logic reading.[^3]

Board mapping comes from Rockbox's target source, whose comments identify B6 as HPWR and B7 as SUSP, describe C1/C2 as charge-disable controls, and tentatively identify B4 as POL and B5 as CHRG. Those tentative signal names require confirmation. A charger-chip datasheet cannot establish the iPod PCB wiring.[^4]

The TEAC manual establishes powered-on charging, standby behavior and video-output support. It does **not** specify which 30-pin power input is used or its available current. The system's 12 V DC input is not proof that 12 V is routed to the iPod's dedicated adapter input. The existing Philips DCP750 adapter exception in the personal source cannot classify the CD-X60i.[^1][^4]

### What can be said about Apple's implementation

The evidence supports an Apple-compatible behavioral target: accept adequate dock power, preserve the hardware charging protections, distinguish battery charging from external-power presence, and cope with a powered accessory changing state. It does not support a claim that Apple uses this tree's backlight probe, that Apple always requests 500 mA from every accessory, or that a particular iAP command unlocks TEAC charging.

A useful independent implementation precedent exists in local Rockbox history. Commit `914760b54e50ba1ef7a0851f09a3035cb71ae14e`, committed February 23, 2026, changes the default to Force on several older iPods. Its author reports slow wall charging and docked playback discharge before the change, and higher input current afterward comparable to stock firmware. Its actual diff changes iPod 4G, Color, Mini 1G/2G, Nano 1G and Video; it does not change the Classic. This is relevant original developer evidence, but its measurements must not be presented as CD-X60i/Classic measurements.[^6]

The repository contains a separate RetailOS 2.0.4 hibernation audit. Its GPIO restore work describes E/F as direct low/high output encodings. It does not audit Apple's charger classification or establish the TEAC handshake. No matching decrypted `osos.fw.decrypted` was located in the accessible home-directory filename search, so no new instruction-level Apple charging audit is claimed here.[^7]

To resolve Apple's exact algorithm later, use an identified lawful firmware image for the actual model, record its hash and relocation map, and trace the callers of B6/B7/C0/C1/C2 writes, B4/B5 reads, USB D+/D− ADC routing, and source-change handlers. Trace both initialization and event-driven updates, including standby/wake. A literal matching a GPIO command without its caller conditions is insufficient evidence. Correlate any reconstruction with a same-device stock-firmware electrical capture.

## Audit of the personal Rockbox tree

### Current budget and settings

`apps/settings_list.c` falls back to `USB_CHARGING_ENABLE` when a target does not define `TARGET_USB_CHARGING_DEFAULT`. The inspected Classic config has no such override. This only establishes the default; an existing saved `usb charging` value overrides it.[^5]

`usb_charging_maxcurrent()` in `firmware/usbstack/usb_core.c` returns:

| Condition | Returned allowance |
|---|---:|
| Core uninitialized or charging disabled | 100 mA |
| USB configured | Current requested for that USB configuration |
| Force selected and `usb_no_host` true | 500 mA |
| Other cases | 100 mA |

The no-host timer is registered for `HZ*10` during core initialization; observing Force for less than ten seconds is not a sufficient test. Actual USB initialization and later host traffic also matter. Capture the returned allowance and callback application, rather than assuming the setting took effect.[^5]

In `power-6g.c:203`, the callback sets B7 according to suspend, B6 according to the 500 mA threshold, and C1 according to either sufficient USB allowance or the dedicated adapter flag. Opening C1 does not raise B6. Thus the screen-on probe can permit charging while leaving the USB input at its smaller allowance.[^4]

### Backlight-dependent control and multiple writers

`power_input_status()` is both a status getter and a controller. A backlight-on edge opens C1 and starts a tick monitor. Sustained monitor behavior latches `usb_charger_detected`; backlight-off can close C1 unless that latch or `usb_high_current_committed` is set. The final adapter branch opens C1 independently.[^4]

This is unsuitable as the long-term source-capability test. Battery-current direction under one load cannot certify an adapter's advertised capacity, and a near-full battery behaves differently from a partially discharged one. Screen state should change load, not permission to use a source. The volatile commitment flag does not make the callback, polling path and resume writes one atomic policy transaction.

There is a further reachable lifecycle issue: removing a dedicated adapter without USB clears the software detection variables but does not explicitly close C1 in that branch. A later source can inherit gate state until another callback or screen transition changes it. This is a stale-state opportunity, not a demonstrated cause of the reported drain.

### Status signal ambiguity

Both `charging_state()` and the tick callback read `PDAT(11) & 0x10`, which is B4. The tick callback calls it CHRG, while initialization comments tentatively call B4 POL and B5 CHRG. The existing B4 read is also present in the public upstream implementation; it is not automatically an off-by-one defect.[^4][^8]

If B4 is POL, the current read could be intentional and more useful for instantaneous direction than CHRG. If the pin is floating or its mapping differs, the reading can be misleading. Instrument both raw inputs and their pull configuration. Do not replace `0x10` with `0x20` merely to make the comments agree.

The initialization comment asserting that PCON E leaves controls floating also conflicts with the repository's GPIO restore implementation, which uses E/F as direct output levels. Explicit GPIO writes can make intent clearer, but that comment is not sufficient proof of a suspended charger at boot. Verify PCON, PDAT and physical behavior before preserving its causal explanation.[^4][^7]

### Percentage and iAP reporting

This target measures battery voltage and uses separate charge/discharge lookup tables. At 3990 mV, its charging table yields 40%, whereas its discharge table yields approximately 81%. This calculation demonstrates that a state-classification change can produce a large percentage jump without equivalent instantaneous energy loss. Sustained matched-load voltage trends or measured battery current are needed to distinguish actual drain from a display transition.[^9]

`iap_fill_power_state()` currently reports external-power status based on `charger_input_state`; it is not a current measurement. General-lingo comments concerning accessory high-power requests describe power for accessories such as RF transmitters. They do not establish incoming battery charging negotiation. Retain protocol semantics unless a TEAC/Apple capture proves a necessary change.[^10]

Video output can change the total load and expose a marginal power allowance. Its availability does not identify the charging rail. Diagnose audio-only and actual video playback separately; do not start by modifying the video driver or audio-buffer lifecycle.

## Ranked explanations and discriminating evidence

| Candidate | Assessment | Evidence that distinguishes it |
|---|---|---|
| USB-only powered dock, unconfigured connection, On setting retains 100 mA | Strongest software lead, conditional on actual input path | Force changes applied B6/current allowance and stops sustained drain |
| Backlight probe/C1 control disables charging | Proven reachable logic, hardware causality unverified | C1 changes with screen state while source and negotiated budget remain constant |
| CD-X60i enters standby or its sleep timer expires | Manufacturer-documented condition | Source changes correlate with TEAC standby; Apple behaves similarly |
| Percentage estimate changes with charging state | Proven possible from tables | Large percentage change without corresponding matched-load voltage decline |
| Invalid B4 bias or misunderstood B4/B5 mapping | Concrete code ambiguity | Raw signal/current correlation contradicts reported direction |
| Weak dock supply/contact, aging battery, or load exceeding supply | Plausible hardware/load alternatives | Apple also fails, input collapses, or failure tracks measured load |
| Missing TEAC-specific iAP power handshake | Currently unsupported | Stock capture shows a required command preceding rail/current change |

## Reproduction and measurement plan

### Existing-firmware check

1. Record iPod model/storage revision, battery modifications, firmware version, saved charging setting, and TEAC regional model. Keep TEAC fully on and disable its sleep timer for the initial comparison.
2. Start with a partially charged battery, preferably around the middle of its usable range. Use one local audio track, fixed volume and matching backlight settings. Avoid initial database scans.
3. Record battery millivolts and the available USB allowance with On selected. Repeat with Force at Settings → General Settings → System → Battery → Charge During USB Connection; the config value is `usb charging: force`. Wait for USB initialization/no-host handling and observe for at least 15–30 minutes, not just until the icon changes.[^5][^11]
4. Restore the original setting after the comparison if Force did not help. Force is a deliberate fallback for a powered charger; it is not proof that an arbitrary weak USB source can supply 500 mA.
5. Repeat the same dock/track comparison in Apple firmware. Then repeat audio-only versus actual video playback, followed by TEAC standby and resume.

The existing hardware debug screen exposes adapter presence, battery voltage, accessory resistance and USB D+/D− voltages. Its ADC routing is active and its lit screen changes load; a debug-screen-only run cannot validate a backlight-related bug. A purpose-built bounded logger should collect the long run without keeping the screen on.[^12]

### Diagnostic record

Use a fixed RAM ring, with sampling in a normal thread, and export after the run. Record timestamp, attach-generation number, reason for policy update, raw/cached adapter presence and validity, USB presence/state/no-host flag, user mode, requested/applied current allowance, PCON11/12, PDAT11/12, B4/B5, B6/B7/C0/C1/C2, backlight/video state, raw/filtered battery mV and reported charge state. Report command and readback separately.

Sample ordinary state at a proposed 1 Hz cadence plus every control transition. If brief current-direction pulses matter, an ISR may increment bounded counters or publish raw bits; it must not perform ADC/I2C work or write logs. Preserve counts rather than a clearable boolean that can lose events. Cadence and filtering are engineering starting points, not Apple-derived constants.

Use a suitable dock breakout and current measurement when software evidence remains ambiguous. Measure the actual incoming rail and, where practicable, battery-current direction. A USB inline meter alone misses dedicated-adapter power and is not a battery-current meter. Do not infer the dock's pin wiring from its mains power rating or video connector.

## Proposed fix specification

### P0 — Establish signal meanings and capture the failure

Add diagnostics before changing current policy. Name unverified inputs `raw_b4` and `raw_b5`; only introduce POL/CHRG semantic names after board or measured evidence confirms them. Determine whether the failure uses USB, dedicated adapter, both, or neither. Preserve an exact failing trace and a corresponding Apple/Force comparison where available.

Acceptance: the report of a failure includes source, allowance, gate state and battery trend, with no reliance on the charging animation. The logger must not materially change the screen-off load or disrupt playback.

### P1 — Minimal current-policy correction

If the CD-X60i supplies USB power and Force resolves the measured drain, the immediate operational fix is the existing Force setting. A minimal firmware candidate is adding `TARGET_USB_CHARGING_DEFAULT USB_CHARGING_FORCE` to the Classic config, matching the approach used for the older iPods in the cited commit. This remains a candidate, not an unconditional default recommendation for every accessory.

Evaluate the battery-powered USB/MFi accessory use cases that motivated the personal C1 gate before shipping that default. Preserve explicit saved On/Off/Force preferences; changing a default does not migrate an existing saved On setting. Document that distinction in release/configuration notes.

If adapter presence is true, changing the USB allowance may be irrelevant. The current tree already opens C1 for that path at initialization, callback, polling and retained resume. Diagnose source validity, C2, real current and load before adding another adapter exception.

Acceptance: on the identified USB-powered TEAC, requested and applied allowance reach the intended value, sustained battery drain stops at the matched load, and host enumeration plus explicit On/Off settings retain their documented behavior.

### P2 — One owner for charge controls

Replace the backlight probe with a deterministic target policy service. USB callbacks publish budget changes; adapter/USB events publish source changes; one normal-thread owner applies B6/B7/C0/C1/C2. `power_input_status()` returns a cached snapshot without mutating controls. Keep a small pure policy function separately testable from GPIO access.

The inputs are source presence/validity, configured USB allowance, suspend condition, user preference, and any *validated* dedicated-charger classification. Backlight and video state are diagnostic load inputs, never source authorization. Source removal increments an epoch so delayed no-host events cannot promote a later attachment using stale evidence.

| Input condition | Proposed policy |
|---|---|
| No valid external source | Clear attachment classification; report battery operation; establish deterministic gate/budget state |
| Dedicated adapter verified present | Permit its charge path independently of USB allowance; retain all hardware protections |
| USB present, no adequate allowance | Preserve conservative input limit and explicit user policy; report external power separately from charge activity |
| USB configured with adequate allowance | Apply the committed limit and permit charging |
| Force + current attachment has no host | Apply existing 500 mA fallback only through the USB service contract |
| USB suspend, no adapter | Honor suspend and avoid a charging claim based on a stale pin sample |
| Adapter removed while USB remains | Recompute from that USB attachment's current policy; do not inherit adapter authorization |
| Resume or cold initialization | Revalidate sources and restore the same policy, with no screen dependency |

The exact low-budget gate policy must retain explicit user semantics and account for accessory-powered operation. Source presence and successful charging are different outputs of this state machine. Do not reset charger timers repeatedly to maintain an icon, bypass thermal safeguards, or enable C0's poorly characterized doubled-current mode.

Apply electrical transitions in a reviewed order: reduce charge demand before reducing/removing its permitted source; establish the new input allowance before opening the charging gate. Keep only required short MMIO sequences atomic. Cached PMU errors must yield an unknown/invalid source observation rather than an invented present/absent transition; audit the existing cached `ADAPTPRES` path accordingly.

### P3 — Honest status and optional automatic classification

Keep external-power presence, current direction, charging permission and full/idle state distinct internally. After validating B4/B5, use the suitable direction signal with valid bias/source context; use CHRG only for the states it actually represents. Define an indeterminate state near zero current. Never label a low battery full merely because CHRG deasserted.

Do not synthesize battery current in mA from voltage or the configured maximum. A sustained voltage decline can support a diagnostic warning under matched load; it cannot become an instantaneous current sensor. Validate charge/discharge table transitions without rewriting battery curves to conceal the original fault.

If automatic USB charger recognition is required, this target already has `adc_read_usbdata_voltage()`. Reuse it only after validating the dock's signature and coordinating exclusive D+/D− routing with the USB driver. Every exit must restore routing, and a live data session must not be disrupted. No fixed Apple divider thresholds are specified because they have not been established for this dock/model. Keep this optional recognition work separate from the minimal current fix.

## Validation and completion criteria

Test the pure policy against attach/detach, delayed events from a prior epoch, adapter+USB combinations, saved settings, suspend, and resume. Build the real Classic target and bootloader where shared target code changes; build a simulator or unaffected target for changes to shared interfaces. Static tests do not validate physical pin mapping or net charge.

| Physical case | Required observation |
|---|---|
| TEAC on, audio, screen on/off | Screen state does not change source authorization; no sustained drain when source is sufficient |
| TEAC on, video | Measured charge/load result compared with audio and Apple baseline; video remains functional |
| TEAC standby and sleep timer | Charging state responds to actual power loss even while still docked |
| TEAC returns on without undock | Automatic reclassification and charging recovery |
| Partially charged and near full | No false full, oscillating authorization or repeated charger reset |
| Host USB and powered charger | Correct enumeration, setting behavior and source transition |
| Battery-powered accessory | No unintended new high-current drain caused by the default change |
| Dedicated adapter and adapter removal with USB retained | Correct independent authorization and fallback |
| Cold boot while docked, retained resume, repeated redock | Deterministic state with no stale attachment latch |

Use a proposed 30-minute steady audio run after settling, repeated at least three times, as a practical acceptance gate. Prefer integrated signed battery-current measurements; otherwise compare matched-load voltage before and after, recognizing temperature and state-of-charge limitations. The final report must state the measurement method and error bounds. Full batteries require maintenance behavior rather than an impossible demand for continually rising voltage.

Do not call the fix verified until the exact final firmware reproduces correct behavior on the CD-X60i. This specification does not include a deployment or upstream submission. Proposed documentation should describe externally observable charging behavior and validated constraints, not claim an exact Apple algorithm that has not been reconstructed.

## Sources

[^1]: TEAC, *CD-X60i instruction manual*, document 77-20CD60I00221, Japanese edition, publication date not established from cover; accessed September 12, 2026. Printed pp. 6–7 (connections) and p. 16 (charging only when on, no standby charging). [Official PDF](https://teac.jp/downloads/teac/852/cd-x60i_om_j_20cd60i00221.pdf). English descriptions here are paraphrased translations. This manual is not a service schematic.

[^2]: Apple, *iPod classic User Guide*, 120 GB edition, 2008-era model, pp. 14–16. [Official PDF](https://cdsassets.apple.com/live/6GJYWVAV/user/ma630_ipod_classic_120gb_en.pdf). Accessed September 12, 2026. Related model documentation, not an executable firmware trace.

[^3]: Linear Technology / Analog Devices, *LTC4066/LTC4066-1 USB Power Manager with Low-Loss Ideal Diode and Li-Ion Battery Charger*, revision C, pp. 8–9, 15–20. [Manufacturer datasheet](https://www.analog.com/media/en/technical-documentation/data-sheets/4066fc.pdf). Accessed September 12, 2026. Chip behavior only; PCB net mapping is separate evidence.

[^4]: Local source, `firmware/target/arm/s5l8702/ipod6g/power-6g.c`, especially `power_init` (line 90), `usb_charging_maxcurrent_change` (203), `chrg_monitor_cb` (239), `power_input_status` (248), and `charging_state` (347); `pmu-6g.c`, `pmu_firewire_present` and `pmu_read_inputs_mbcs` (327–335). Personal working-tree snapshot identified above. Local file access; current source is not proof of installed firmware.

[^5]: Local source, `firmware/usbstack/usb_core.c`, `usb_no_host_callback` (183), initialization (557–558), `usb_charging_maxcurrent` (1456); `firmware/usb.c`, `USB_CHARGER_UPDATE` handling and `usb_charger_update`; `apps/settings_list.c` default selection (457–460) and charging setting (2235–2238); `firmware/export/config/ipod6g.h`. Personal working-tree snapshot, September 12, 2026.

[^6]: Rockbox Git history, Paul Sauro, *config: USB_CHARGING_FORCE must be enabled for all iPods*, commit `914760b54e50ba1ef7a0851f09a3035cb71ae14e`; authored February 13, 2025, committed February 23, 2026. Message and six-file diff inspected in the local Git object database. Stock-performance comparison is the author's report, not independent Classic hardware validation.

[^7]: Local `tools/ipod6g_stock_hibernate_audit.py` (image identity/load map and GPIO audit), `firmware/target/arm/s5l8702/gpio-s5l8702.c` (`gpio_hibernate_suspend/resume`), and `docs/ipod6g-hibernate-deep-research/report-source.md`. These describe a separate RetailOS 2.0.4 hibernation analysis. The identified reference image SHA-256 is `f4368251a58b2fdc7b46acf3178dae1d24bc1e029736240741015851256c65c4`; that image was not newly inspected for this charging report.

[^8]: Rockbox project, current upstream `power-6g.c`, accessed September 12, 2026. [Source](https://raw.githubusercontent.com/Rockbox/rockbox/master/firmware/target/arm/s5l8702/ipod6g/power-6g.c). Upstream uses the same B4 status read but lacks this personal tree's backlight-gated classification. [Current Classic configuration](https://raw.githubusercontent.com/Rockbox/rockbox/master/firmware/export/config/ipod6g.h) also has no Force-default override at access time. These master URLs are moving references.

[^9]: Local `firmware/target/arm/s5l8702/ipod6g/powermgmt-6g.c`, charge/discharge lookup tables; `firmware/powermgmt.c`, `voltage_to_battery_level`; `firmware/export/config/ipod6g.h`, voltage-measurement configuration. Percentage example is calculated from the inspected tables, not measured on hardware.

[^10]: Local `apps/iap/iap-core.c`, `iap_fill_power_state` (2387); `apps/iap/iap-lingo0.c`, introductory power-negotiation comments. No TEAC-specific power handshake capture was available.

[^11]: Rockbox manual source, local `manual/configure_rockbox/system_options.tex`, Battery / Charge During USB Connection, lines 49–56. Describes Force for USB AC adapters without a data connection.

[^12]: Local `firmware/target/arm/s5l8702/debug-s5l8702.c`, hardware debug inputs/ADC section; `firmware/target/arm/s5l8702/ipod6g/adc-6g.c`, `adc_read_usbdata_voltage` (89). ADC routing is an active hardware operation, not passive observation.
