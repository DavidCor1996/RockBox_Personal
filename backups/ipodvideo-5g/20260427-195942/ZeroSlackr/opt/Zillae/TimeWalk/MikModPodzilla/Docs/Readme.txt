Building and using MikModPodzilla for uClinux/iPod
    Scott Lawrence
    ipod@umlautllama.com
    2005 Apr 04

This file contains instructions to build mikpodzilla (BUILDING), 
instructions for just installing a prebuilt version (INSTALL),
and instructions for using it (USING)


== CREDIT ==

This port wouldn't be possible without all of the hard work of the
Jean-Paul Mikkers (MikMak), the rest of the MikMod team, and the
podzilla team.


For more information about MikMod:
    MikMod: 		http://mikmod.raphnet.net/
    MikMod For Unix:	http://www.tfn.net/~amstpi/mikmod.html

The "MikMod for unix" page is included, even though it offers a much
older version of Mikmod (3.0.3 instead of 3.2.x), since that's the port
that I based this work on.



== BUILDING ==

These are the instructions for building MikPodzilla for the
iPod/uCLinux.  It is implied that you have already followed the
directions on the ipodLinux wiki, and set up a developers envioronment.

	building podzilla: http://www.ipodlinux.org/Building_Podzilla

Before continuing any further, you should make sure that you can
build Podzilla, and run it on your iPod.

It should be noted that there are issues with some of the older,
installed versions of the uCLinux kernel with MikMod.  You should
grab the nightly build of the kernel and install it

	nightly build: 	http://www.ipodlinux.org/builds/

	installation:   http://www.ipodlinux.org/Kernel_Building

CD into your development tree directory

	cd yourSourceDirectory

A directory listing should show something like this:

	ipp
	libitunesdb
	libjpeg
	microwindows
	podzilla

Once you are there, rename your podzilla directory to something
else, to get it out of the way.

	mv podzilla podzilla.1

Next, ungzip/untar the mikpodzilla source:

	gzip -cd mkpd_2005-04-04.tar.gz | tar xvf - 

Now, we need to build MikMod.

	cd mikmod/ipod/
	./build-mikmod.ipod.sh

Now, change back over into the podzilla directory to build mikmodpodzilla

	cd ../../podzilla
	make IPOD=1

you should have a 'podzilla' executable in there that you can copy to
the bin directory on your ipod.

	cp podzilla /Volumes/ipod/bin/


== INSTALLING ==

If you instead want to just drop the binary in place, do the following

	gunzip mikpodzilla.gz
	chmod 755 mikpodzilla
	cp mikpodzilla /Volumes/ipod/bin/

You can then run regular podzilla, and navigate to your /bin
directory, and launch mikpodzilla.

You do, however still need to have a fairly recent kernel installed. 
(See above).


== USING ==

In order to play songs, you will need to add them to a playlist.  By
default, MikMod will look for "default.mik" in your root ipod directory.
What I do, is find all mods, and just dump them into there.

	cd /Volumes/ipod/
	find . -name "*.mod" > default.mik

Remember to do this for other formats as well..

	find . -name "*.xm" >> default.mik
	find . -name "*.s3m" >> default.mik

etc.

The file browser in podzilla has been modified to understand that
files ending in ".mik" are MikMod playlists.   Like I mentioned
earlier, if you launch MikMod from the main podzilla menu, it will
just load the "default.mik" playlist.  If you want to load other
playlists, navigate to the .mik playlist via the file browser, and
press the action button.

The display shows:
    Playlist Name
    Filename of song playing
    Title of song playing
    Playback position information
    Spinner mode
    [ Volume Slider ]
    [ Pattern list position ]
    [ Pattern row position ]

The Playback position information contains the following fields:
    row:nrows   pat/npats   patno

Each music module song consists of an ordering of [npats] patterns.
Each pattern has [nrows] rows.  Each [row] contains the notes and effects
to be played at that moment in time.

The patterns can be arranged in any order.  [pat] will always show the
current entry number in the complete list of patterns.  This will always
increase from 0 (first pattern in list) to whatever npats is.  If the
composer wanted to, they could have a song consisting of 34 entries of
pattern number 3.  [pat]/[npats] would display  0/34 then 1/34 and so on,
while [patno] will display 3, the actual number of the pattern playing.

The Spinner mode is changed by pressing the action (centre) button.
This will change what action the spinner performs.  (adjust volume,
Scrub through the song, or nothing)



Enjoy!

	-Scott (ipod@umlautllama.com)
