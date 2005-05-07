R1Q2 Client README (short version)
==================================
Thanks for downloading R1Q2! This file is a very quick introduction to some of
the major changes in the R1Q2 client. Please check out the full client readme at
http://www.r1ch.net/forum/index.php?topic=105.0 for more details on the new
features, commands and cvars.

Things You Need
===============
It's strongly recommended, although not required, that you use R1GL, my enhanced
renderer for Quake II. It supports things like PNG/JPG images, alpha pics,
full-scene anti-aliasing, anisotropic filtering, security fixes and much more.
You can get it from http://www.r1ch.net/stuff/r1gl/ - extract to your Q2 dir.

The R1Q2 Updater available on the website is also strongly recommended. Run it
periodically every few weeks to automatically keep all your R1Q2 related files
up to date. In fact, you should probably run it right now to be sure you really
have the latest versions of everything. For more information see the ChangeLog
at http://www.r1ch.net/forum/index.php?topic=106 for what's new in each release.

"I just want to get it working like NoCheat!"
=============================================
Here's some config settings you can use then.

set cl_cmdcomplete 0 //restore default command completion behaviour
set vid_ref "r1gl" //if using R1GL; R1Q2 is also able to load NCGL and plain GL
set m_fixaccel 1 //replaces m_xp of NoCheat, also see m_directinput
set cl_async 0 //disable asynchronous fps

Please be aware that R1Q2 will load ALL .pak files from your baseq2 and mod 
directories. Rename or move away any paks you don't want loaded.

Important Changes You Should Be Aware Of
========================================
The R1Q2 client uses asynchronous FPS by default. This means that the speed at
which graphics are updated (renderer FPS) is separate from the rate at which
packets are sent to the server (network or packet FPS). Most modern hardware is
capable of running Q2 at crazy rendering FPS which on traditional clients will
cause many packets to be sent to the server resulting in increased upstream
bandwidth usage on the client, increased downstream and higher CPU usage on the
server and additional workload for any routers and other devices that have to
shift your packets around.

The way in which R1Q2 separates the net/renderer FPS causes multiple frames
worth of input to be sent in a single packet. Due to how the Q2 physics engine
works, this will result in small differences in the way movement is processed.
The most notable effect is the height that you are able to attain when jumping
off ramps or other types of jump. To most users, this won't be noticable.
Another issue that occurs is that strafe jumping may "feel" different - some
players report a "sticky" effect as if they are stuck to the floor for a brief
moment after jumping. If you don't notice either of these issues then great -
you can use the asynchronous FPS and save both yours and the servers resources.
If however you experience "weird" movement, you can disable this asynchronous
FPS using the "cl_async" cvar. Simply add in your config set cl_async 0 to turn
it off. When using the asynchronous FPS, cl_maxfps will control the NETWORK
packet rate whilst r_maxfps will control your rendering rate.

That's pretty much one of the major changes to the client. Another feature most
people immediately want to get working is location reporting. R1Q2 uses
NoCheat-style $$loc_here and $$loc_there cvars (note the two $ signs is
intentional, R1Q2 "meta-vars" all start with $$). If used with other characters
next to them, expand by adding braces, eg ${$loc_here}. Location info is read
from the following locations: moddir/locs/mapname.loc, moddir/maps/mapname.loc,
baseq2/locs/mapname.loc, baseq2/maps/mapname.loc and are in a format compatible
with most other clients.

OpenAL audio was recently introduced to the R1Q2 client. OpenAL is an audio 
library similiar to how OpenGL works for graphics. It allows fully positional 3D 
audio, providing a more realistic sound experience. There are a few quirks with 
it at present, notably looping sounds don't synchronize with each other and the 
falloff from entity sounds is not quite the same as standard audio. To use the 
OpenAL audio, you must first install the OpenAL drivers, available from 
http://developer.creative.com/articles/article.asp?cat=1&sbcat=31&top=38&aid=46.
Then add to your autoexec.cfg 'set s_initsound 2' to have R1Q2 use OpenAL upon
startup. You can also set the s_openal_device cvar to DirectSound or MMSYSTEM
if your sound card drivers have issues with DS3D (the default).

R1Q2 will load all .pak files by default. If you have any pak files you don't 
want to be loaded, move them away or rename them to something other than .pak. 
R1Q2 will load the standard pakXX.pak followed by the remaining pak files in 
alphabetical order. Remember that files in paks that are loaded last are used 
first, ie pak3.pak overrides pak0.pak, sounds.pak overrides pak9.pak, etc.

For more information, please see the full readme on the forums:
http://www.r1ch.net/forum/index.php?topic=105.0

If you experience any problems with R1Q2 such as crashes or odd behaviour,
please search the forums first for a possible solution, if nothing similar turns
up, post a bug report. You don't to register to post on the R1Q2 forums.

Links
=====
R1Q2 Homepage: http://www.r1ch.net/stuff/r1q2/
R1GL Homepage: http://www.r1ch.net/stuff/r1gl/
R1Q2 Forums: http://www.r1ch.net/forum/index.php?board=8.0

"Getting R1Q2 to work proper": http://cato.troligt.com/filer/r1q2.htm
"How to install R1Q2": http://www.kontula.net/comments.php?id=145
"r1q2 faq" (English/Estonian): http://r1q2.quake2.ee/
