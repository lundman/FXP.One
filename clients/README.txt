
This is hopefully the future space for a few, small clients that will
use the FXP.One engine. Hopefully there will also be larger
independent projects as well, but shipped separately. 

I myself, intent, and hope to see the following projects.


<short>    :  <description>

siteedit   : Small application, perhaps ncurses or similar, that will
           : do site additions, site modification and deletion. I
		   : imagine that most clients will have a site "workshop" but
		   : it would also be useful to have a small stand-alone.

clomps     : Connected to multiple FTP sites, listing configured
           : directories, to compare and compute new releases, using
           : both internal, and site's skip and
           : passlists. Additionally create a matrix of sites to show
           : where the new items are missing, incomplete, skipped or
           : already synchronised. 

lmirror    : lmirror replacement client. Connect to two remote FTP
           : sites, and compute new entries, ignoring skiplists, and
           : build queues for transfer.

FXP.One    : Commandline, ncurses and CDK GUI. Mostly complete,
           : lacking some eye-candy and features. Must be built
           : against latest version of CDK at invisible-island.net.
		   : It will not work with CDK version that ships with *BSD,
		   : Linux. http://invisible-island.net/cdk/

GUI-FXP    : GUI, WxWidgets? GTK? MFC? GUI FXP client. Glen Malley has
           : implemented a Python-Gtk GUI but his sources are not part
           : of this repository.





** SITE KEY/PAIR VALUES.

FXP.One lets any client define their own variables that can be stored
in the site database with all the standard information. However,
occasionally it might be desirable that clients support the same names
for over-lapping functionality. Therefor I present a list here that
client programmers can chose to follow. If you wish to store your own
key/pair variables, that is perfectly fine.



------------/-------------:--------------------------------------------------
KEY NAME    | VALUE TYPE  : Description
------------|-------------:--------------------------------------------------
STARTDIR    | <str>       : If set, client will CWD to <str> upon connect.
            |             : For example "/incoming/".
------------|-------------:--------------------------------------------------
USE_XDUPE   | <yna>       : Attempt to use X-DUPE with site. Default
            |             : is AUTO and will only send on "FEAT"
			|			  : reply. Set to YES or NO to force feature on/off
------------|-------------:--------------------------------------------------
			|			  : 
------------/-------------:--------------------------------------------------

 



