# Restore Classic TV controls in the personal source tree

The September 23 Classic image (04984f554eM-260923) was built from the personal tree without the TV presentation changes developed in the September 19 task. Its settings registry and menus did not contain TV Screen, Fit to Screen, or TV Overscan.

Restore the previously tested TV implementation through revision f3bd518, merging with the current personal files. Keep the newer RetailOS asset handling and the 32-entry app icon cache. Add a direct Composite Video entry to the RetailOS Settings list; the same submenu remains under General Settings > Display > Composite Video.

Controls include Standard 4:3 / Widescreen 16:9, Fit to Screen, TV Overscan, TV Interface, TV Text Size, TV Now Playing, TV UI Sounds, TV test, and dock remote controls. Existing configuration keys remain unchanged. Fit crops only video while menus use the selected aspect and safe margins. Accessory detection does not select aspect.

Restore the supporting Apple TV Home, app rendering, native video presentation, and remote fixes together, so settings remain connected to their implementation. The known dock-side audible volume issue is not claimed fixed by this restoration.

Package the larger TV application icons in ordinary personal builds. Skip device-bound video capability generation when installing the simulator, which has no rockbox.ipod image.

Hardware composite behavior still requires user testing. Device installation needs explicit user approval; leave the device mounted afterward.
