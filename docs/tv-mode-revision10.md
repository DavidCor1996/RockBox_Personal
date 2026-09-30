# TV Home activation and Live TV preview ownership

Home now checks for TV activation in the running handheld dashboard loop. It saves the current handheld selection, stops LCD scrolling, and returns through the root dispatcher with the native theme lifecycle balanced. Re-entry selects the dedicated TV Home loop immediately, including its category tabs and remote navigation, without requiring an app launch and without nesting dashboard stack frames.

The Classic LCD YUV mirror now honors semantic TV ownership and batching, as the RGB mirror already does. Handheld picture-in-guide updates are consumed without writing or presenting their LCD-coordinate rectangle on TV. The TV guide continues to draw the decoded picture into its own inset preview rectangle. LCD drawing and the native full-screen video frame path remain intact.

Validation includes the production Home dispatch branches (100 late activations plus 100 initially docked entries), the entire production YUV mirror function (60,000 suppressed thumbnail updates with an unchanged scanout, 100 releases restoring mirroring, native frame and inactive paths), existing TV ownership/Home/renderer tests, Classic/Video/simulator builds, and active-music navigation regression. Physical dock and composite output require device testing.

No plugin API, art, media, database, guide controls, or remote mappings change. Leave the installed Classic mounted.
