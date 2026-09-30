# Guide scrolling without volume changes

Keep a dock's navigation cursor separate from audio gain when subsequent volume synchronization arrives. Previously an unmarked volume update cleared the navigation cursor and copied its value into global audio volume. Once a navigation gesture establishes a cursor, both volume synchronization forms now acknowledge it without changing gain or producing an extra button. Initial synchronization, mute, non-navigation mode, USB audio and Kokkia retain their existing paths.

The current saved remote trace predates this report, so it does not establish which synchronization form the dock used during the reported incident. Regression replay covers both forms after captured directional packet bursts, including transaction IDs, and checks that six taps still give six steps at unchanged volume. The guide event loop separately checks Up/Down and volume-coded Up/Down, repeats and releases, with Select/Play and held Left preserved.

Native Classic build and focused tests precede installation. Broader playback tests are deferred to the user's physical test per their request. Leave the iPod mounted.
