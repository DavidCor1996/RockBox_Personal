You will need to copy some additional files onto your iPod:

*	MultiConvert.tabl to /etc/
*	SeaChelUnicode.fnt to /usr/share/fonts/ (OPTIONAL)
*	ptextwords to /usr/share/dict/ (OPTIONAL)
*	ptext.db to /etc/ (OPTIONAL)

If you have a FAT32-formatted iPod on Windows, just launch
INSTALL.BAT and it will copy all this stuff for you.

Some tcsh shell scripts have been provided that will be useful:
*	applypatch.sh: Provided the path to a podzilla source
	directory, patches that directory with the text input system.
	Must be executed from the TextInput8 directory.
*	makepz.sh: Builds a clean copy of podzilla-ti with SQLite.
	Must be executed from a podzilla source directory.
*	makediff.sh: Creates the text_input.diff file.
	Must be executed from a podzilla source directory.
*	makepkg.sh: Provided a directory path, creates a text input
	package in that directory, ready to be compressed and
	uploaded to the iPodLinux wiki.
	Must be executed from a podzilla source directory.


There are six entry points to the text input system:

new_text_window_np();
	/* opens a text input window with no callback */
new_text_window_nppw();
	/* opens a password text input window with no callback */
new_text_window(x, y, width, height, text, callback);
	/* opens a normal text input window */
new_text_window_password(x, y, width, height, text, callback);
	/* opens a password text input window */
new_text_window_numeric(x, y, width, height, text, callback);
	/* opens a numeric text input window */
new_text_window_x(x, y, width, height, text, callback,
draw, output, handler, exit);
	/* opens a text input window with custom handlers */


Here are the bug fixes and enhancements made in TextInput8:

*	Removed need for podwrite.h file.
*	Renamed textinputextensions.c to run.c (since all it
	contained was new_run_window).
*	Added functions for clickwheel control in clickwheel.h
	and clickwheel.c. These can be used by anything in podzilla,
	not just text input.
*	Made center button on Four-Button Keyboard return to home
	level (A-H, I-P, Q-X, YZEtc) when pressed, exit when held
	down.
*	Now includes SQLite port with minimal features.
	This is an optional component specified with PTEXT_DB=1
	passed to the make command (see next item).
*	Predictive text uses SQLite database. This will only work
	with PTEXT_DB=1 passed to the make command; otherwise
	predictive text uses the text dictionary file as before.
*	Improved reliability of Telephone Keypad and Thumbscript
	input methods though clickwheel.c. You should now be able
	to just touch the wheel instead of rubbing it.
*	Made PodWrite a little faster by not redrawing the header
	on every refresh. (This is most noticable when you have
	decorations turned on.)
*	A unified function for a text input method to display
	a message inside the text input window.
*	New text input entry point for passworded input.
*	Four-button keyboard has better layout for status bar
	on non-3g iPods.
*	Text window is now centered vertically if passed 0 for y.
*	PodWrite displays confirmation dialog before clearing
	all text.
*	PodWrite remembers the name of a file opened through the
	file browser or the name of the file last saved when
	using the Save command.
*	Fixed a couple memory leaks, and a bus error that would
	occur when leaving the Save text box empty.
*	Text input always exits if it receives a $1B (escape)
	keystroke. This is helpful if you're running podzilla in X11.


Here are the bug fixes and enhancements made in TextInput7:

*	Serial, Morse Code, Cursive, and Keypad input methods
	now call both text_output_char(10) and text_exit(), so
	we are now able to make a distinction between the two.
	(Other methods already had this distinction.)
*	PodWrite no longer uses the hold button, but instead
	uses the distinction between newline and exit. See
	the text input mappings / text input summary file.
*	Telephone Keypad input methods support more characters
	under different buttons, instead of just basic ASCII
	all under the 1 button.
*	Four-Button Keypad can now move the cursor (at the
	sacrifice of having space and backspace under the 1 key).
*	Morse Code can now move the cursor. The codes to do this
	are .-... (for left) and .-..- (for right).
*	Morse Code now covers all of ASCII. The codes for this
	will be listed separately.
*	WheelBoard can now move the cursor. It's implemented
	as another "character set."
*	Scroll Through and Dial Type can now move the cursor
	through an M option at the end of the character list.
*	Finally fixed the problem with WheelBoard not using the
	correct font.
*	New text input methods: Telephone Keypad and Thumbscript
	use the click wheel on 4th-gen iPods as a telephone keypad.
	Telephone Keypad inputs characters as on a cellular phone,
	and Thumbscript inputs characters using Thumbscript (see
	information and reference at http://www.thumbscript.com).
	See jackinloadup's post about these concepts at
	http://ipodlinux.org/forums/viewtopic.php?p=4194#4194
	for more information.
	Someone will have to test these for me because I don't
	have a fourth-generation iPod.
*	Predictive Text! Turn on ptext, the iPod predictive text
	system, by holding down the action button in telephone
	keypad modes. The 1 key acts the same, but the 2-9 keys
	now input characters immediately. You must have a file
	/usr/share/dict/ptextwords for this to work.


Here are the bug fixes and enhancements made in TextInput6:

*	Arrays of chars instead of literal strings used for
	temporary storage (preventing segmentation faults).


Here are the bug fixes and enhancements made in TextInput5:

*	Works with latest CVS (August 21, 2005).
*	Internationalization support.
*	Podzilla-ti has updated libitunesdb.


Here are the bug fixes and enhancements made in TextInput4:

*	Works with latest CVS (July 25, 2005).
*	Text input, MultiConvert, and PodWrite are all appearance-
	compliant (use the colors from appearance.h).
*	Text input, MultiConvert, and PodWrite are all double-
	buffered (no more flickering at all).
*	Brackets and currency are switched in Four-Button Keyboard,
	and an @ sign is added to the symbols.


Here are the bug fixes and enhancements made in TextInput3:

*	Mouse cursor is moved back to the middle of the screen
	after the text window closes, fixing a potential bug
	if a text window was way off-center and a message window
	was displayed.
*	MultiConvert is included.
*	Numeric mode, used by MultiConvert.
*	"Dead keys" in On-Screen Keyboard now use correct case.
*	On-Screen Keyboard will use SeaChelUnicode.fnt (included)
	or SeaChel.fnt for the keyboard, if they are found in
	/usr/share/fonts. This is a tiny font ideal for the small
	buttons of the On-Screen Keyboard.
*	Non-ASCII characters in Four-Button Keyboard no longer
	act funky.
*	Text input buffer is now dynamic rather than fixed-size.
*	Telephone Keypad can now generate a zero.
*	Better handling of text input methods cleaning up on exit.
*	Hold switch in Telephone Keypad input no longer attempts
	to generate characters.
*	Four-Button Keyboard and Cursive methods can move the cursor.
*	PodWrite stops clicking and redrawing text when reaching
	the beginning or end while in move or scroll mode.
*	Improved scrolling in PodWrite (hopefully).
*	Rewind and forward move cursor to beginning or end of text
	in move mode in PodWrite.
*	PodWrite now displays a scroll bar.
*	No compiler warnings whatsoever.


Here are the bug fixes and enhancements made in TextInput2:

*	Fix to keypad.c incorporated.
*	Rename option in File Browser menu.
*	Hooks for overriding four important functions in the
	text input system.
*	Proper management of graphic context (gc).
*	Text input uses the same font as the rest of podzilla.
*	Fewer compiler warnings.
*	Other miscellaneous bug fixes.
*	On-Screen Keyboard text input method (really crowded though).
*	UTF-8 support (only On-Screen Keyboard currently outputs
	non-ISO-Latin-1 characters).
*	Text cursor (caret, insertion point) display.
*	Editing in the middle of the string.
*	PodWrite, a Podzilla word processor.
*	Open in PodWrite option in File Browser menu.

