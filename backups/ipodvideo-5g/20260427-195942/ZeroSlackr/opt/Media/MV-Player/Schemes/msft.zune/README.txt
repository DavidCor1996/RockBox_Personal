Zune scheme for MV Player (Preview)
----------------------------------

This is about as close as you can get to making your iPod look and function like a Zune, the Microsoft portable media player.

The scheme is modeled after the second-generation interface of the Zune, which uses large text on its main menu. For comparison, see the image at
http://tinyurl.com/2jzypc.

The font used in this scheme is a direct copy of 'Zegoe UI,' the exact font used on the Zune.


Although this looks pretty neat, this definitely doesn't make your iPod as cool as a Zune. :)

Do yourself a favor...buy one!

Check out www.zune.net for more info. 



Installation
-------------------------------------------------------------------------

This assumes that you already have iPodLinux, MV player Preview version, and podzilla (legacy) installed on your iPod.

You can find more information about MV Player at http://ipodlinux.org/Mv_player, and more schemes for it at
http://ipodlinux.org/Mv_player/Schemes.

I was only able to test this on a first-gen nano, but it should work fine on anything compatible with MV Player.


1. Copy the entire msft.zune folder to your /schemes directory.

2. Edit your mvpd.conf file to use this theme. Your file should look a bit like this:

--------------------------------------------
Scheme: /hp/Videos/schemes/msft.zune/
movies@/hp/Videos/movies/
music videos@/hp/Videos/musicvideos/
tv shows@/hp/Videos/shows/
--------------------------------------------

Remember to change 'hp' to 'mnt' if your iPod mounts on mnt (it's probably mnt if you use it with a Mac).


I reccomend that you use only lowercase letters for menu titles (i.e. 'movies' as opposed to 'Movies'), because that's how the Zune menus are done.


*** Important: Because of the unusually large font size in this theme, the 'folder empty' dialogue (that normally shows up if a folder has no videos) does not display. If you click on an empty folder, you just get a blank screen. Just press center again to go back.








Thanks to Gaspode for developing MV Player!








And, just for the heck of it (although this isn't really software)...

THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESSED OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.



