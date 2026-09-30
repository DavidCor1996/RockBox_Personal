# Test build: Live TV guide controls and native video ownership

Select and Play/Pause in full-screen Live TV route to the existing guide session; held Left in the guide retains its normal exit to Home. Other players and Desktop controls are unchanged.

MPEG and core H264 presenters keep native display ownership between frames, including pause and control-hide transitions. Borrowed decoder plane pointers are still copied synchronously and cleared by the driver. Explicit hide/cleanup, guide/PIN/weather handoffs, and core UI release restore LCD ownership. This prevents unrelated LCD bottom-strip refreshes from temporarily overwriting the native TV picture.

The user requested immediate deployment for their own testing due to low credits. Run the native build and focused ownership/control harnesses; defer broader simulator/playback regression and physical black-bar confirmation to the user. Leave the Classic mounted. No audio, database, media, or plugin API changes.
