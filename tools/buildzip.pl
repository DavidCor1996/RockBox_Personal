#!/usr/bin/perl
#             __________               __   ___.
#   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
#   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
#   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
#   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
#                     \/            \/     \/    \/            \/
# $Id$
#

use strict;

use File::Copy; # For move() and copy()
use File::Find; # For find()
use File::Path qw(mkpath rmtree); # For rmtree()
use Cwd;
use Cwd 'abs_path';
use Getopt::Long qw(:config pass_through);    # pass_through so not confused by -DTYPE_STUFF

my $ROOT="..";

my $ziptool;
my $output;
my $verbose;
my $install;
my $exe;
my $target;
my $modelname;
my $incfonts;
my $target_id; # passed in, not currently used
my $rbdir; # can be non-.rockbox for special builds
my $app;
my $mklinks;

sub glob_mkdir {
    my ($dir) = @_;
    mkpath ($dir, $verbose, 0777);
    return 1;
}

sub glob_install {
    my ($_src, $dest, $opts) = @_;

    unless ($opts) {
        $opts = "-m 0664";
    }

    foreach my $src (glob($_src)) {
        unless ( -d $src || !(-e $src)) {
            system("install $opts \"$src\" \"$dest\"");
            print "install $opts \"$src\" -> \"$dest\"\n" if $verbose;
        }
    }
    return 1;
}

sub glob_copy {
    my ($pattern, $destination) = @_;
    print "glob_copy: $pattern -> $destination\n" if $verbose;
    foreach my $path (glob($pattern)) {
        copy($path, $destination);
    }
}

sub tree_copy {
    my ($src, $dest, $skip) = @_;
    return unless -d $src;

    find({ wanted => sub {
        my $path = $File::Find::name;
        return if $path eq $src;

        my $rel = $path;
        $rel =~ s/^\Q$src\E\/?//;

        if (defined $skip && $rel =~ $skip) {
            $File::Find::prune = 1 if -d $path;
            return;
        }

        my $target = "$dest/$rel";

        if (-d $path) {
            glob_mkdir($target);
        } else {
            my $target_dir = $target;
            $target_dir =~ s/\/[^\/]+$//;
            glob_mkdir($target_dir);
            copy($path, $target);
        }
    }, no_chdir => 1 }, $src);
}

sub copy_sitekick_assets {
    my ($dest) = @_;
    my $src = "$ROOT/assets/ipodjs/rockbox/sitekick";
    return unless -d $src;

    glob_mkdir($dest);
    glob_mkdir("$dest/base");
    glob_mkdir("$dest/chips");
    glob_mkdir("$dest/icons");
    glob_mkdir("$dest/data");
    glob_mkdir("$dest/sounds");
    glob_mkdir("$dest/backgrounds");
    glob_mkdir("$dest/preview");
    glob_mkdir("$dest/desktop");
    glob_mkdir("$dest/state");
    glob_mkdir("$dest/sync");
    copy("$src/source.manifest", $dest);
    glob_copy("$src/base/*.bmp", "$dest/base");
    glob_copy("$src/chips/*.bmp", "$dest/chips");
    glob_copy("$src/icons/*.bmp", "$dest/icons");
    glob_copy("$src/data/*.tsv", "$dest/data");
    glob_copy("$src/sounds/*.uib", "$dest/sounds");
    glob_copy("$src/backgrounds/*.bmp", "$dest/backgrounds");
    glob_copy("$src/preview/*.bmp", "$dest/preview");
    glob_copy("$src/desktop/*.rga", "$dest/desktop");
}

sub copy_clubpenguin_assets {
    my ($dest) = @_;
    my $src = "$ROOT/assets/ipodjs/rockbox/clubpenguin";
    return unless -d $src;

    glob_mkdir($dest);
    glob_mkdir("$dest/covers");
    glob_mkdir("$dest/data");
    glob_mkdir("$dest/rooms");
    glob_mkdir("$dest/shop");
    glob_mkdir("$dest/ui");
    glob_mkdir("$dest/minigames");
    glob_mkdir("$dest/minigames/cart_surfer");
    copy("$src/world.bmp", $dest);
    copy("$src/player.bmp", $dest);
    copy("$src/source.manifest", $dest);
    glob_copy("$src/covers/*.bmp", "$dest/covers");
    glob_copy("$src/data/*.tsv", "$dest/data");
    glob_copy("$src/rooms/*.bmp", "$dest/rooms");
    glob_copy("$src/shop/*.bmp", "$dest/shop");
    glob_copy("$src/ui/*.bmp", "$dest/ui");
    copy("$src/ui/source.manifest", "$dest/ui");
    glob_copy("$src/minigames/cart_surfer/*.bmp",
              "$dest/minigames/cart_surfer");
    copy("$src/minigames/cart_surfer/source.manifest",
         "$dest/minigames/cart_surfer");
}

sub glob_move {
    my ($pattern, $destination) = @_;
    print "glob_move: $pattern -> $destination\n" if $verbose;
    foreach my $path (glob($pattern)) {
        move($path, $destination);
    }
}

sub glob_unlink {
    my ($pattern) = @_;
    print "glob_unlink: $pattern\n" if $verbose;
    foreach my $path (glob($pattern)) {
        unlink($path);
    }
}

sub find_copyfile {
    my ($pattern, $destination) = @_;
    print "find_copyfile: $pattern -> $destination\n" if $verbose;
    return sub {
        my $path = $_;
        my $source = getcwd();
        if ($path =~ $pattern && filesize($path) > 0 && !($path =~ /$rbdir/)) {
            if($mklinks) {
                print "link $path $destination\n" if $verbose;
                symlink($source.'/'.$path, $destination.'/'.$path);
            } else {
                print "cp $path $destination\n" if $verbose;
                copy($path, $destination);
                chmod(0755, $destination.'/'.$path);
            }
        }
    }
}

sub find_installfile {
    my ($pattern, $destination) = @_;
    print "find_installfile: $pattern -> $destination\n" if $verbose;
    return sub {
        my $path = $_;
        if ($path =~ $pattern) {
        print "FIND_INSTALLFILE: $path\n";
            glob_install($path, $destination);
        }
    }
}


sub make_install {
    my ($src, $dest) = @_;

    my $bindir = $dest;
    my $libdir = $dest;
    my $userdir = $dest;

    my @plugins = ( "games", "apps", "demos", "viewers" );
    my @userstuff = ( "backdrops", "codepages", "docs", "fonts", "langs", "themes", "wps", "eqs", "icons" );
    my @files = ();

    if ($app) {
        $bindir .= "/bin";
        $libdir .= "/lib/rockbox";
        $userdir .= "/share/rockbox";
    } else {
        # for non-app builds we expect the prefix to be the dir above .rockbox
        $bindir .= "/$rbdir";
        $libdir = $userdir = $bindir;
    }
    if ($dest =~ /\/dev\/null/) {
        die "ERROR: No PREFIX given\n"
    }

    if ((!$app) && -e $bindir && -e $src && (abs_path($bindir) eq abs_path($src))) {
        return 1;
    }

    # binary
    unless ($exe eq "") {
        unless (glob_mkdir($bindir)) {
            return 0;
        }
        glob_install($exe, $bindir, "-m 0775");
    }

    # codecs
    unless (glob_mkdir("$libdir/codecs")) {
        return 0;
    }
    # Android has codecs installed as native libraries so they are not needed
    # in the zip.
    if ($modelname !~ /android/) {
        glob_install("$src/codecs/*", "$libdir/codecs", "-m 0755");
    }

    # plugins
    unless (glob_mkdir("$libdir/rocks")) {
        return 0;
    }
    foreach my $t (@plugins) {
        unless (glob_mkdir("$libdir/rocks/$t")) {
            return 0;
        }
        glob_install("$src/rocks/$t/*", "$libdir/rocks/$t", "-m 0755");
    }

    if(-e "$src/rocks/games/sgt_puzzles") {
        unless (glob_mkdir("$libdir/rocks/games/sgt_puzzles")) {
            return 0;
        }
        glob_install("$src/rocks/games/sgt_puzzles/*", "$libdir/rocks/games/sgt_puzzles", "-m 0755");
    }

    # rocks/viewers/lua
    unless (glob_mkdir("$libdir/rocks/viewers/lua")) {
        return 0;
    }
    glob_install("$src/rocks/viewers/lua/*", "$libdir/rocks/viewers/lua");

    #lua include scripts
    if(-e "$ROOT/apps/plugins/lua/include_lua") {
        unless (glob_mkdir("$libdir/rocks/viewers/lua")) {
            return 0;
        }
        glob_install("$ROOT/apps/plugins/lua/include_lua/*.lua", "$libdir/rocks/viewers/lua");
        #glob_mkdir("$temp_dir/rocks/viewers/lua");
        #glob_copy("$ROOT/apps/plugins/lua/include_lua/*.lua", "$temp_dir/rocks/viewers/lua/");
    }

    #lua example scripts
    if(-e "$ROOT/apps/plugins/lua_scripts") {
        unless (glob_mkdir("$libdir/rocks/demos/lua_scripts")) {
            return 0;
        }
        glob_install("$ROOT/apps/plugins/lua_scripts/*.lua", "$libdir/rocks/demos/lua_scripts");
        #glob_mkdir("$temp_dir/rocks/demos/lua_scripts");
        #glob_copy("$ROOT/apps/plugins/lua_scripts/*.lua", "$temp_dir/rocks/demos/lua_scripts/");
    }

    #mujs example scripts
    if(-e "$ROOT/apps/plugins/mujs/scripts") {
        unless (glob_mkdir("$libdir/scripts/data")) {
            return 0;
        }
        glob_install("$ROOT/apps/plugins/mujs/scripts/*.js", "$libdir/scripts");
        glob_install("$ROOT/apps/plugins/mujs/scripts/data/*", "$libdir/scripts/data");
    }

    #lua picross puzzles
    if(-e "$ROOT/apps/plugins/picross") {
        unless (glob_mkdir("$libdir/rocks/games/.picross")) {
            return 0;
        }
        glob_install("$ROOT/apps/plugins/picross/*.picross", "$libdir/rocks/games/.picross");
    }

    # all the rest directories
    foreach my $t (@userstuff) {
        unless (glob_mkdir("$userdir/$t")) {
            return 0;
        }
        glob_install("$src/$t/*", "$userdir/$t");
    }

    # wps/ subfolders and bitmaps
    opendir(DIR, $src . "/wps");
    @files = readdir(DIR);
    closedir(DIR);

    foreach my $_dir (@files) {
        my $dir = "wps/" . $_dir;
        if ( -d "$src/$dir" && $_dir !~ /\.\.?/) {
            unless (glob_mkdir("$userdir/$dir")) {
                return 0;
            }
            glob_install("$src/$dir/*", "$userdir/$dir");
        }
    }

    # rest of the files, excluding the binary
    opendir(DIR,$src);
    @files = readdir(DIR);
    closedir(DIR);

    foreach my $file (grep (/[a-zA-Z]+\.(txt|config|ignore|sh)/,@files)) {
        glob_install("$src/$file", "$userdir/");
    }
    return 1;
}

# Get options
GetOptions ( 'r|root=s'      => \$ROOT,
             'z|ziptool:s'   => \$ziptool,
             'm|modelname=s' => \$modelname,  # The model name as used in ARCHOS in the root makefile
             'i|id=s'        => \$target_id,  # The target id name as used in TARGET_ID in the root makefile
             'o|output:s'    => \$output,
             'f|fonts=s'     => \$incfonts,   # 0 - no fonts, 1 - fonts only 2 - fonts and package
             'v|verbose'     => \$verbose,
             'install=s'     => \$install, # install destination
             'rbdir:s'       => \$rbdir, # If we want to put in a different directory
             'l|link'        => \$mklinks, # If we want to create links instead of copying files
             'a|app:s'       => \$app, # Is this an Application build?
    );

# GetOptions() doesn't remove the params from @ARGV if their value was ""
# Thus we use the ":" for those for which we use a default value in case of ""
# and assign the default value afterwards
if ($ziptool eq '') {
    $ziptool = "zip -r9";
}
if ($output eq '') {
    $output = "rockbox.zip"
}
if ($rbdir eq '') {
    $rbdir = ".rockbox";
}

# Now @ARGV shuold be free of any left-overs GetOptions left
($target, $exe) = @ARGV;

my $firmdir="$ROOT/firmware";
my $appsdir="$ROOT/apps";
my $viewer_bmpdir="$ROOT/apps/plugins/bitmaps/viewer_defaults";

my $cppdef = $target;

sub gettargetinfo {
    open(GCC, ">gcctemp");
    # Get the LCD screen depth and graphical status
    print GCC <<STOP
\#include "config.h"
Bitmap: yes
Depth: LCD_DEPTH
LCD Width: LCD_WIDTH
LCD Height: LCD_HEIGHT
Icon Width: CONFIG_DEFAULT_ICON_WIDTH
Icon Height: CONFIG_DEFAULT_ICON_HEIGHT
#ifdef HAVE_REMOTE_LCD
Remote Depth: LCD_REMOTE_DEPTH
Remote Icon Width: CONFIG_REMOTE_DEFAULT_ICON_WIDTH
Remote Icon Height: CONFIG_REMOTE_DEFAULT_ICON_HEIGHT
#else
Remote Depth: 0
#endif
#ifdef HAVE_RECORDING
Recording: yes
#endif
STOP
;
    close(GCC);

    my $c="cat gcctemp | gcc $cppdef -I. -I$firmdir/export -E -P -";

    # print STDERR "CMD $c\n";

    open(TARGET, "$c|");

    my ($bitmap, $width, $height, $depth, $icon_h, $icon_w);
    my ($remote_depth, $remote_icon_h, $remote_icon_w);
    my ($recording);
    my $icon_count = 1;
    while(<TARGET>) {
        # print STDERR "DATA: $_";
        if($_ =~ /^Bitmap: (.*)/) {
            $bitmap = $1;
        }
        elsif($_ =~ /^Depth: (\d*)/) {
            $depth = $1;
        }
        elsif($_ =~ /^LCD Width: (\d*)/) {
            $width = $1;
        }
        elsif($_ =~ /^LCD Height: (\d*)/) {
            $height = $1;
        }
        elsif($_ =~ /^Icon Width: (\d*)/) {
            $icon_w = $1;
        }
        elsif($_ =~ /^Icon Height: (\d*)/) {
            $icon_h = $1;
        }
        elsif($_ =~ /^Remote Depth: (\d*)/) {
            $remote_depth = $1;
        }
        elsif($_ =~ /^Remote Icon Width: (\d*)/) {
            $remote_icon_w = $1;
        }
        elsif($_ =~ /^Remote Icon Height: (\d*)/) {
            $remote_icon_h = $1;
        }
        if($_ =~ /^Recording: (.*)/) {
            $recording = $1;
        }
    }
    close(TARGET);
    unlink("gcctemp");

    return ($bitmap, $depth, $width, $height, $icon_w, $icon_h, $recording,
            $remote_depth, $remote_icon_w, $remote_icon_h);
}

sub filesize {
    my ($filename)=@_;
    my ($dev,$ino,$mode,$nlink,$uid,$gid,$rdev,$size,
        $atime,$mtime,$ctime,$blksize,$blocks)
        = stat($filename);
    return $size;
}


sub buildzip {
    my ($image, $fonts)=@_;
    my $libdir = $install;
    my $temp_dir = ".rockbox";

    print "buildzip: image=$image fonts=$fonts\n" if $verbose;

    my ($bitmap, $depth, $width, $height, $icon_w, $icon_h, $recording,
        $remote_depth, $remote_icon_w, $remote_icon_h) =
      &gettargetinfo();

    # print "Bitmap: $bitmap\nDepth: $depth\n";

    # remove old traces
    rmtree($temp_dir);

    glob_mkdir($temp_dir);

    if(!$bitmap) {
        # always disable fonts on non-bitmap targets
        $fonts = 0;
    }
    if($fonts) {
        glob_mkdir("$temp_dir/fonts");
        chdir "$temp_dir/fonts";
        my $cmd = "$ROOT/tools/convbdf -f $ROOT/fonts/*bdf >/dev/null 2>&1";
        print($cmd."\n") if $verbose;
        system($cmd);
        copy("$ROOT/fonts/COPYING", "COPYING-fonts.txt");
        chdir("../../");

        if($fonts < 2) {
          # fonts-only package, return
          return;
        }
    }

    # create the file so the database indexer skips this folder
    open(IGNORE, ">$temp_dir/database.ignore")  || die "can't open database.ignore";
    close(IGNORE);
    # create the file so the talkclip generation skips this folder
    open(IGNORE, ">$temp_dir/talkclips.ignore")  || die "can't open talkclips.ignore";
    close(IGNORE);
    # create the file so bookmark generation skips this folder
    open(IGNORE, ">$temp_dir/bookmark.ignore")  || die "can't open bookmark.ignore";
    close(IGNORE);

    # the samsung ypr0 has a loader script that's needed in the zip
    if ($modelname =~ /samsungypr[01]/) {
        glob_copy("$ROOT/utils/ypr0tools/rockbox.sh", "$temp_dir/");
    }
    # add .nomedia on Android
    # in the zip.
    if ($modelname =~ /android/) {
        open(NOMEDIA, ">$temp_dir/.nomedia")  || die "can't open .nomedia";
        close(NOMEDIA);
    }
    # copy wifi firmware
    if ($modelname =~ /sansaconnect/) {
        glob_mkdir("$temp_dir/libertas");
        glob_copy("$ROOT/firmware/drivers/libertas/firmware/*", "$temp_dir/libertas/");
    }
    # add hbmenu shortcut's icon and 3dsx executable
    if ($modelname =~ /ctru/) {
        glob_copy("icon.icn", "$temp_dir/");
        glob_copy("rockbox.3dsx", "$temp_dir/");
    }

    glob_mkdir("$temp_dir/langs");
    glob_mkdir("$temp_dir/rocks");
    glob_mkdir("$temp_dir/rocks/games");
    glob_mkdir("$temp_dir/rocks/apps");
    glob_mkdir("$temp_dir/rocks/demos");
    glob_mkdir("$temp_dir/rocks/viewers");

    if ($recording) {
        glob_mkdir("$temp_dir/recpresets");
    }

    glob_mkdir("$temp_dir/eqs");
    glob_copy("$ROOT/lib/rbcodec/dsp/eqs/*.cfg", "$temp_dir/eqs/"); # equalizer presets

    glob_mkdir("$temp_dir/wps");
    glob_mkdir("$temp_dir/icons");
    glob_mkdir("$temp_dir/themes");
    glob_mkdir("$temp_dir/codepages");

    if($bitmap) {
        system("$ROOT/tools/codepages");
    }
    else {
        system("$ROOT/tools/codepages -m");
    }

    glob_move('*.cp', "$temp_dir/codepages/");

    if($bitmap && $depth > 1) {
        glob_mkdir("$temp_dir/backdrops");
    }

    glob_mkdir("$temp_dir/codecs");

    # Android has codecs installed as native libraries so they are not needed
    # in the zip.
    if ($modelname !~ /android/) {
        find(find_copyfile(qr/.*\.codec/, abs_path("$temp_dir/codecs/")), 'lib/rbcodec/codecs');
    }

    # remove directory again if no codec was copied
    rmdir("$temp_dir/codecs");

    find(find_copyfile(qr/\.(rock|ovl|lua)/, abs_path("$temp_dir/rocks/")), 'apps/plugins');

    # PicoDrive is an explicitly enabled, non-commercial personal-use port.
    # RockPod installs it directly; never let a stale opt-in build artifact
    # leak into a normal Rockbox distribution archive.
    glob_unlink("$temp_dir/rocks/picodrive.rock");

    #lua include scripts
    if(-e "$ROOT/apps/plugins/lua/include_lua") {
        glob_mkdir("$temp_dir/rocks/viewers/lua");
        glob_copy("$ROOT/apps/plugins/lua/include_lua/*.lua", "$temp_dir/rocks/viewers/lua/");
    }

    #lua example scripts
    if(-e "$ROOT/apps/plugins/lua_scripts") {
        glob_mkdir("$temp_dir/rocks/demos/lua_scripts");
        glob_copy("$ROOT/apps/plugins/lua_scripts/*.lua", "$temp_dir/rocks/demos/lua_scripts/");
    }

    #mujs example scripts
    if(-e "$ROOT/apps/plugins/mujs/scripts") {
        glob_mkdir("$temp_dir/scripts/data");
        glob_copy("$ROOT/apps/plugins/mujs/scripts/*.js", "$temp_dir/scripts/");
        glob_copy("$ROOT/apps/plugins/mujs/scripts/data/*", "$temp_dir/scripts/data/");
    }

    #lua picross puzzles
    if(-e "$ROOT/apps/plugins/picross") {
        glob_mkdir("$temp_dir/rocks/games/.picross");
        glob_copy("$ROOT/apps/plugins/picross/*.picross", "$temp_dir/rocks/games/.picross/");
    }

    # exclude entries for the image file types not supported by the imageviewer for the target.
    my $viewers = "$ROOT/apps/plugins/viewers.config";
    my $c="cat $viewers | gcc $cppdef -I. -I$firmdir/export -E -P -include config.h -";

    open VIEWERS, "$c|" or die "can't open viewers.config";
    my @viewers = <VIEWERS>;
    close VIEWERS;

    open VIEWERS, ">$temp_dir/viewers.config" or
        die "can't create $temp_dir/viewers.config";

    foreach my $line (@viewers) {
        if ($line =~ /([^,]*),([^,]*),/) {
            my ($ext, $plugin)=($1, $2);
            my $r = "${plugin}.rock";
            my $oname;

            my $dir = $r;
            my $name;

            # strip off the last slash and file name part
            $dir =~ s/(.*)\/(.*)/$1/;
            # store the file name part
            $name = $2;

            # get .ovl name (file part only)
            $oname = $name;
            $oname =~ s/\.rock$/.ovl/;

            # print STDERR "$ext $plugin $dir $name $r\n";

            if(-e "$temp_dir/rocks/$name") {
                if($dir ne "rocks") {
                    # target is not 'rocks' but the plugins are always in that
                    # dir at first!
                    move("$temp_dir/rocks/$name", "$temp_dir/rocks/$r");
                }
                print VIEWERS $line;
            }
            elsif(-e "$temp_dir/rocks/$r") {
                # in case the same plugin works for multiple extensions, it
                # was already moved to the viewers dir
                print VIEWERS $line;
            }

            if(-e "$temp_dir/rocks/$oname") {
                # if there's an "overlay" file for the .rock, move that as
                # well
                move("$temp_dir/rocks/$oname", "$temp_dir/rocks/$dir");
            }
        }
    }
    close VIEWERS;

    open CATEGORIES, "$ROOT/apps/plugins/CATEGORIES" or
        die "can't open CATEGORIES";
    my @rock_targetdirs = <CATEGORIES>;
    close CATEGORIES;
    foreach my $line (@rock_targetdirs) {
        if ($line =~ /([^,]*),(.*)/) {
            my ($plugin, $dir)=($1, $2);
            if($dir  eq 'games' and substr(${plugin}, 0, 4) eq "sgt-") {
                glob_mkdir("$temp_dir/rocks/$dir/sgt_puzzles");
                move("$temp_dir/rocks/${plugin}.rock", "$temp_dir/rocks/$dir/sgt_puzzles/${plugin}.rock");
            }
            else {
                move("$temp_dir/rocks/${plugin}.rock", "$temp_dir/rocks/$dir/${plugin}.rock");
            }
            if(-e "$temp_dir/rocks/${plugin}.ovl") {
                # if there's an "overlay" file for the .rock, move that as
                # well
                move("$temp_dir/rocks/${plugin}.ovl", "$temp_dir/rocks/$dir");
            }
            if(-e "$temp_dir/rocks/${plugin}.lua") {
                # if this is a lua script, move it to the appropriate dir
                move("$temp_dir/rocks/${plugin}.lua", "$temp_dir/rocks/$dir/");
            }
        }
    }

    # Maps ships with its fixed-size, offline satellite frames.  They are
    # loaded once by the plugin and never fetched at runtime.
    if($width == 320 && $height == 240 &&
       -e "$temp_dir/rocks/apps/nb_maps.rock") {
        mkpath("$temp_dir/maps", $verbose, 0777);
        copy("$ROOT/assets/nb_maps/world_satellite.r16",
             "$temp_dir/maps/world_satellite.r16");
        foreach my $globe_frame (glob("$ROOT/assets/nb_maps/world_globe_*.r16")) {
            my $globe_name = $globe_frame;
            $globe_name =~ s!^.*/!!;
            copy($globe_frame, "$temp_dir/maps/$globe_name");
        }
        copy("$ROOT/assets/nb_maps/maritime_satellite.r16",
             "$temp_dir/maps/maritime_satellite.r16");
        copy("$ROOT/assets/nb_maps/moncton_imagery.rgb",
             "$temp_dir/maps/moncton_imagery.rgb");
        foreach my $dashcam (glob("$ROOT/assets/nb_maps/moncton_dashcam_*.rgb")) {
            my $dashcam_name = $dashcam;
            $dashcam_name =~ s!^.*/!!;
            copy($dashcam, "$temp_dir/maps/$dashcam_name");
        }
        copy("$ROOT/assets/nb_maps/fredericton_imagery.rgb",
             "$temp_dir/maps/fredericton_imagery.rgb");
        copy("$ROOT/assets/nb_maps/fredericton_panoramax_0.rgb",
             "$temp_dir/maps/fredericton_panoramax_0.rgb");
        copy("$ROOT/assets/nb_maps/fredericton_panoramax_1.rgb",
             "$temp_dir/maps/fredericton_panoramax_1.rgb");
        copy("$ROOT/assets/nb_maps/saint_john_imagery.rgb",
             "$temp_dir/maps/saint_john_imagery.rgb");
        foreach my $panorama (glob("$ROOT/assets/nb_maps/*_360_*.rgb"),
                              glob("$ROOT/assets/nb_maps/times_square_*.rgb")) {
            my $panorama_name = $panorama;
            $panorama_name =~ s!^.*/!!;
            copy($panorama, "$temp_dir/maps/$panorama_name");
        }
        copy("$ROOT/assets/nb_maps/london_kartaview.rgb",
             "$temp_dir/maps/london_kartaview.rgb");
        copy("$ROOT/assets/nb_maps/berlin_kartaview.rgb",
             "$temp_dir/maps/berlin_kartaview.rgb");
        copy("$ROOT/assets/nb_maps/paris_panoramax.rgb",
             "$temp_dir/maps/paris_panoramax.rgb");
        copy("$ROOT/assets/nb_maps/toronto_panoramax_0.rgb",
             "$temp_dir/maps/toronto_panoramax_0.rgb");
        copy("$ROOT/assets/nb_maps/toronto_panoramax_1.rgb",
             "$temp_dir/maps/toronto_panoramax_1.rgb");
        copy("$ROOT/assets/nb_maps/toronto_panoramax_2.rgb",
             "$temp_dir/maps/toronto_panoramax_2.rgb");
        copy("$ROOT/assets/nb_maps/tokyo_panoramax.rgb",
             "$temp_dir/maps/tokyo_panoramax.rgb");
        my @world_tiles;
        find(sub {
            push @world_tiles, $File::Find::name
                if -f $_ && $_ =~ /\.r16$/;
        }, "$ROOT/assets/nb_maps/world_tiles");
        foreach my $source (@world_tiles) {
            my $relative = $source;
            $relative =~ s!^\Q$ROOT/assets/nb_maps/world_tiles/\E!!;
            my $destination = "$temp_dir/maps/world/$relative";
            my $directory = $destination;
            $directory =~ s!/[^/]+$!!;
            mkpath($directory, $verbose, 0777);
            copy($source, $destination);
        }
    }

    glob_unlink("$temp_dir/rocks/*.lua"); # Clean up unwanted *.lua files (e.g. actions.lua, buttons.lua)

    copy("$ROOT/apps/tagnavi.config", "$temp_dir/");
    copy("$ROOT/apps/plugins/disktidy.config", "$temp_dir/rocks/apps/");

    if(-e "$temp_dir/rocks/viewers/open_plugins.rock") {
        my $cwd = getcwd();
        copy("$cwd/apps/plugins/open_plugins.opx", "$temp_dir/rocks/apps/open_plugins.opx") or
            print STDERR "Copy failed: $cwd/apps/plugins/open_plugins.opx $!\n";
    }

    if($bitmap) {
        copy("$ROOT/apps/plugins/sokoban.levels", "$temp_dir/rocks/games/sokoban.levels"); # sokoban levels
        copy("$ROOT/apps/plugins/snake2.levels", "$temp_dir/rocks/games/snake2.levels"); # snake2 levels
        copy("$ROOT/apps/plugins/rockbox-fonts.config", "$temp_dir/rocks/viewers/");
        # picross files
        copy("$ROOT/apps/plugins/picross_default.picross", "$temp_dir/rocks/games/picross_default.picross");
        copy("$ROOT/apps/plugins/bitmaps/native/picross_numbers.bmp",
             "$temp_dir/rocks/games/picross_numbers.bmp");
    }

    if(-e "$temp_dir/rocks/demos/pictureflow.rock") {
        copy("$ROOT/apps/plugins/bitmaps/native/pictureflow_emptyslide.100x100x16.bmp",
             "$temp_dir/rocks/demos/pictureflow_emptyslide.bmp");
        my ($pf_logo);
        if ($width < 200) {
            $pf_logo = "pictureflow_logo.100x18x16.bmp";
        } else {
            $pf_logo = "pictureflow_logo.193x34x16.bmp";
        }
        copy("$ROOT/apps/plugins/bitmaps/native/$pf_logo",
             "$temp_dir/rocks/demos/pictureflow_splash.bmp");
        if ($width == 320 && $height == 240) {
            copy("$ROOT/apps/plugins/bitmaps/native/pictureflow_loading_bg.320x240x24.bmp",
                 "$temp_dir/rocks/demos/pictureflow_loading_bg.bmp");
        }

    }

    if(-e "$temp_dir/rocks/games/rockboy_launcher.rock") {
        mkpath("$temp_dir/rocks/games/rockboy_launcher/covers", $verbose, 0777);
        mkpath("$temp_dir/games/library/covers/systems", $verbose, 0777);
        if ($width == 320 && $height == 240) {
            copy("$ROOT/apps/plugins/bitmaps/native/rockboy_loading_bg.320x240x24.bmp",
                 "$temp_dir/rocks/games/rockboy_launcher/loading_bg.bmp");
        }
        copy("$ROOT/apps/plugins/bitmaps/native/doom_cover.120x140x24.bmp",
             "$temp_dir/rocks/games/rockboy_launcher/covers/Doom.bmp");
        copy("$ROOT/assets/game_covers/systems/flash.bmp",
             "$temp_dir/games/library/covers/systems/flash.bmp");
        if(-e "$ROOT/assets/game_covers/snes/Killer Instinct (USA) (Rev 1).bmp") {
            mkpath("$temp_dir/games/library/covers/snes", $verbose, 0777);
            copy("$ROOT/assets/game_covers/snes/Killer Instinct (USA) (Rev 1).bmp",
                 "$temp_dir/games/library/covers/snes/Killer Instinct (USA) (Rev 1).bmp");
        }
        if(-e "$temp_dir/rocks/games/sm64.rock" ||
           -e "$temp_dir/rocks/games/sm64.ovl") {
            mkpath("$temp_dir/games/library/covers/n64", $verbose, 0777);
            copy("$ROOT/assets/game_covers/n64/n64-system.bmp",
                 "$temp_dir/games/library/covers/systems/n64.bmp");
            copy("$ROOT/assets/game_covers/n64/Super Mario 64 (USA).bmp",
                 "$temp_dir/games/library/covers/n64/Super Mario 64 (USA).bmp");
        }
    }

    # The optional iPodJS Steam library uses only verifiable, non-placeholder
    # native-game artwork: official Rockbox manual screenshots or existing
    # title artwork recorded in SOURCES.tsv. The firmware discovers these
    # covers beside the plugins that actually made it into this build.
    if(-d "$temp_dir/rocks/games" &&
       -e "$ROOT/assets/game_covers/native/SOURCES.tsv") {
        my %steam_native_copied;
        open(my $steam_sources, '<',
             "$ROOT/assets/game_covers/native/SOURCES.tsv") or
            die "can't open native game cover sources";
        while(my $line = <$steam_sources>) {
            chomp($line);
            my ($plugin, $kind, $source) = split(/\t/, $line, 3);
            next if !defined($plugin) || $plugin eq 'plugin';
            next unless $kind eq 'rockbox-manual-screenshot' ||
                        $kind eq 'existing-cover';
            next if $steam_native_copied{$plugin};
            next unless -e "$temp_dir/rocks/games/$plugin.rock";
            next unless -e "$ROOT/assets/game_covers/native/$plugin.bmp";
            mkpath("$temp_dir/games/library/covers/native",
                   $verbose, 0777);
            copy("$ROOT/assets/game_covers/native/$plugin.bmp",
                 "$temp_dir/games/library/covers/native/$plugin.bmp");
            $steam_native_copied{$plugin} = 1;
        }
        close($steam_sources);
    }

    if(-e "$temp_dir/rocks/games/pokemini_launcher.rock" &&
       -d "$ROOT/assets/ipodjs/rockbox/pokemini/covers") {
        mkpath("$temp_dir/rocks/games/pokemini_launcher/covers",
               $verbose, 0777);
        glob_copy("$ROOT/assets/ipodjs/rockbox/pokemini/covers/*.bmp",
                  "$temp_dir/rocks/games/pokemini_launcher/covers");
    }

    if(-e "$temp_dir/rocks/games/clubpenguin.rock" &&
       -d "$ROOT/assets/ipodjs/rockbox/clubpenguin") {
        copy_clubpenguin_assets("$temp_dir/rocks/games/clubpenguin");
    }

    if(-e "$temp_dir/rocks/apps/sitekick.rock" &&
       -d "$ROOT/assets/ipodjs/rockbox/sitekick") {
        copy_sitekick_assets("$temp_dir/sitekick");
    }

    if($image) {
        # image is blank when this is a simulator
        if( filesize("rockbox.ucl") > 1000 ) {
            copy("rockbox.ucl", "$temp_dir/rockbox.ucl");  # UCL for flashing
        }
        if( filesize("rombox.ucl") > 1000) {
            copy("rombox.ucl", "$temp_dir/rombox.ucl");  # UCL for flashing
        }

        # Check for rombox.target
        if ($image=~/(.*)\.(\w+)$/)
        {
            my $romfile = "rombox.$2";
            if (filesize($romfile) > 1000)
            {
                copy($romfile, "$temp_dir/$romfile");
            }
        }
    }

    glob_mkdir("$temp_dir/docs");
    for(("COPYING",
         "LICENSES",
         "KNOWN_ISSUES"
        )) {
        copy("$ROOT/docs/$_", "$temp_dir/docs/$_.txt");
    }
    if ($fonts) {
        copy("$ROOT/docs/profontdoc.txt", "$temp_dir/docs/profontdoc.txt");
    }
    for(("sample.colours",
         "sample.icons"
        )) {
        copy("$ROOT/docs/$_", "$temp_dir/docs/$_");
    }

    # Now do the WPS dance
    if(-d "$ROOT/wps") {
        my $wps_build_cmd="perl $ROOT/wps/wpsbuild.pl ";
        $wps_build_cmd=$wps_build_cmd."-v " if $verbose;
        $wps_build_cmd=$wps_build_cmd." --tempdir=$temp_dir --rbdir=$rbdir -r $ROOT -m $modelname $ROOT/wps/WPSLIST $target";
        print "wpsbuild: $wps_build_cmd\n" if $verbose;
        system("$wps_build_cmd");
        print "wps_build_cmd: done\n" if $verbose;
    }
    else {
        print STDERR "No wps module present, can't do the WPS magic!\n";
    }

    # until buildwps.pl is fixed, manually copy the classic_statusbar theme across
    mkdir "$temp_dir/wps/classic_statusbar", 0777;
    glob_copy("$ROOT/wps/classic_statusbar/*.bmp", "$temp_dir/wps/classic_statusbar");
    if ($depth >= 16 && $height > 480) {
        copy("$ROOT/wps/classic_statusbar.24.sbs", "$temp_dir/wps/classic_statusbar.sbs");
    } elsif ($depth == 16) {
        copy("$ROOT/wps/classic_statusbar.sbs", "$temp_dir/wps");
    } elsif ($depth > 1) {
        copy("$ROOT/wps/classic_statusbar.grey.sbs", "$temp_dir/wps/classic_statusbar.sbs");
    } else {
        copy("$ROOT/wps/classic_statusbar.mono.sbs", "$temp_dir/wps/classic_statusbar.sbs");
    }
    if ($remote_depth != $depth) {
        copy("$ROOT/wps/classic_statusbar.mono.sbs", "$temp_dir/wps/classic_statusbar.rsbs");
    } else {
        copy("$temp_dir/wps/classic_statusbar.sbs", "$temp_dir/wps/classic_statusbar.rsbs");
    }
    copy("$temp_dir/wps/rockbox_none.sbs", "$temp_dir/wps/rockbox_none.rsbs");

    if(-d "$ROOT/assets/ipodjs/rockbox") {
        tree_copy("$ROOT/assets/ipodjs/rockbox", "$temp_dir/ipodjs",
                  qr{^clubpenguin(?:/|$)|^sitekick(?:/|$)|(?:^|/)\.rockbox(?:/|$)|^(?:24-iLike\.fnt|alphabet-overlay-stock\.|status-(?:battery|playing|hold|header|repeat|shuffle)-stock\.|volume_(?:left|right)_stock\.)});
        copy_clubpenguin_assets("$temp_dir/ipodjs/clubpenguin");
        # The iPodJS Hold screen uses this larger stock-like clock face.  It
        # must be in FONT_DIR on hardware or the renderer falls back to the
        # much smaller menu font.
        copy("$ROOT/fonts/35-Adobe-Helvetica-Bold.fnt", "$temp_dir/fonts");
    }
    if(-d "$ROOT/assets/ipodjs/apple") {
        # Apple binaries are private and gitignored.  When the verified
        # extraction tool has prepared them, preserve their explicit apple/
        # namespace so iPodJS never mistakes third-party theme art for stock.
        tree_copy("$ROOT/assets/ipodjs/apple", "$temp_dir/ipodjs/apple");
    }

    if(-d "$ROOT/apps/plugins/offlineweb_seed/.rockbox/offlineweb") {
        tree_copy(
            "$ROOT/apps/plugins/offlineweb_seed/.rockbox/offlineweb",
            "$temp_dir/offlineweb"
        );
    }

    # and the info file
    copy("rockbox-info.txt", "$temp_dir/rockbox-info.txt");

    # copy the already built lng files
    glob_copy('apps/lang/*.lng', "$temp_dir/langs/");
    glob_copy('apps/lang/*.zip', "$temp_dir/langs/");
    # Copy over the Invalid Language fallback stuff
    glob_copy("$ROOT/apps/lang/Invalid*.talk", "$temp_dir/langs/");

    # Copy over any generated voice/talk clips
    glob_copy('Invalid*.talk', "$temp_dir/langs/");
    glob_copy('*.lng.talk', "$temp_dir/langs/");
    glob_copy('*.voice', "$temp_dir/langs/");

    # copy the .lua files
    glob_mkdir("$temp_dir/rocks/viewers/lua/");
    glob_copy('apps/plugins/lua/*.lua', "$temp_dir/rocks/viewers/lua/");
}

my ($sec,$min,$hour,$mday,$mon,$year,$wday,$yday,$isdst) =
 localtime(time);

$mon+=1;
$year+=1900;

#$date=sprintf("%04d%02d%02d", $year,$mon, $mday);
#$shortdate=sprintf("%02d%02d%02d", $year%100,$mon, $mday);

# made once for all targets
sub runone {
    my ($target, $fonts)=@_;

    # Strip the leading / from $rbdir unless we are installing an application
    # build - the layout is different (no .rockbox, but bin/lib/share)
    unless ($app && $install) {
        $rbdir = substr($rbdir, 1);
    }

    # build a full install .rockbox ($rbdir) directory
    buildzip($target, $fonts);

    unlink($output);

    if($fonts == 1) {
        # Don't include image file in fonts-only package
        undef $target;
    }
    if($target && ($target !~ /(mod|ajz|wma)\z/i)) {
        # On some targets, the image goes into .rockbox.
        copy("$target", ".rockbox/$target");
        undef $target;
    }

    if($install) {
        if($mklinks) {
            my $cwd = getcwd();
            symlink("$cwd/.rockbox", "$install/.rockbox");
            print "link .rockbox $install\n" if $verbose;
        } else {
            make_install(".rockbox", $install) or die "MKDIRFAILED\n";
            rmtree(".rockbox");
            print "rm .rockbox\n" if $verbose;
        }
    }
    else {
        unless (".rockbox" eq $rbdir) {
            mkpath($rbdir);
            rmtree($rbdir);
            move(".rockbox", $rbdir);
            print "mv .rockbox $rbdir\n" if $verbose;
        }

        # add hbmenu shortcut and cia file to zip file
        if ($modelname =~ /ctru/) {
            move("rockbox.cia", "3ds");
            copy("$ROOT/packaging/ctru/rockbox.xml", "3ds");

            system("$ziptool -u $output 3ds/rockbox.xml $target >/dev/null");
            print "$ziptool $output $ROOT/packaging/ctru/rockbox.xml $target >/dev/null\n" if $verbose;
            system("$ziptool -u $output 3ds/rockbox.cia $target >/dev/null");
            print "$ziptool $output rockbox.cia $target >/dev/null\n" if $verbose;
        }

        system("$ziptool $output $rbdir $target >/dev/null");
        print "$ziptool $output $rbdir $target >/dev/null\n" if $verbose;
        rmtree("$rbdir");
        print "rm $rbdir\n" if $verbose;
    }
};

if(!$exe) {
    # not specified, guess!
   if($target =~ /iriver/i) {
        $exe = "rockbox.iriver";
    }
}
elsif(($exe =~ /rockboxui/)) {
    # simulator, exclude the exe file
    $exe = "";
}
elsif($exe eq "librockbox.so") {
    # android, exclude the binary
    $exe="";
}

runone($exe, $incfonts);
