
Welcome to the FXP.One project.


Before you start, this is not just another FTP/FXP client. It does not
work that way. It is much better!


FXP.One consists of an engine, and
you would need (at least) one GUI to control it.


The engine will need OpenSSL if you want to support SSL/TLS.

The FXP.Cone GUI will need ncurses and CDK.
Latest CDK can be found here: http://invisible-island.net/ with
latest package, at the time of this document:
# ftp://invisible-island.net/cdk/cdk.tar.gz



********************************************************************
*** FXP.One engine
********************************************************************

The idea was to put all the hard code into an engine that is really
easy to interface with. This has worked surprisingly well, in that you
can FTP/FXP entirely by using "telnet". This means writing a client,
which is essentially just a Display Shell, quite trivial. This
includes people who wants to just script clients, like by using perl,
python and so on.

!! This also means you do not have to run the engine on the same machine
   that you run the GUI client(s). !!

For example, if you had a stable host, perhaps even with a static IP
making the credentials for authentication easier, you could run the
engine there. Then connect to it by the client of you choice.

You can disconnect this client (the engine will keep working if you do
not tell it to stop) and start a new (and possibly different) client
elsewhere, and resume control of your previous work.

But you can run the engine and client at the same time, and quitting
at the same time, making it appear like a normal FTP/FXP client.



** Compiling the engine.

% ./configure
% make

If you compile from CVS repository sources, you need to rebuild the
autoconf files first.

% autoreconf
% ./configure
% make

If it is being difficult, also try:

% autoreconf --force --install



********************************************************************
** Running the engine.
********************************************************************

Since the engine will be saving your remote FTP site information, as
well as any User/Passwords you chose on disk, you should consider
encrypting these files.

FXP.One engine will do this for you. When you start the engine, it
will as for a "Key". This is the Key that it will use to encrypt all
its data files. If you do not want encryption, just press return.

For the first time you run FXP.One, just make up whatever Key you
want.

When you start the engine in future, you have to specify the correct
Key or it will not be able to read your data files. (And you will not
be able to login).

To start again, you need to delete all .FXP.One files, and the engine
will create these again.

When the user file does not exist, the FXP.One engine will
automatically create a login as "admin" with password "admin".


NOTE: If you edit .FXP.One.settings file, you can change the option
"ssl=1". By default it will only allow SSL connections. If you change
it to "ssl=0" you make it optional. This allows you to telnet to the
engine should you wish to check it out. Please read the
"engine/API.txt" for more information on the FXP.One protocol.

NOTE: Please be aware that all skiplist, movefirstlist and passlists
are separated by the "/" character.
That is, "fmovefirst=*.sfv/*.nfo".





********************************************************************
** The FXP.One clients
********************************************************************

At the time of this document, there are only three clients available.


* FXP.One client

Probably the most complete engine is the ncurses engine. It uses CDK
and ncurses. but please note that should your OS come with CDK, it is
most likely not current enough to work.

% cd clients/FXP.One/
% gmake

Please read the README.txt with the client.


* FXP.One clomps

I made a console only application that will connect to multiple FTP
sites, work out what is new on each one of them, then build a matrix
displaying where they exist, are incomplete or missing. Optionally,
you can also specify a search pattern to find where there are hits.

This client assumes you have already defined your FXP.One sites in the
engine ready to use.

% cd clients/clomps/
% gmake


* UFxp

GUI / Ultimate++ client, please see the URL:

http://lundman.net/wiki/index.php/UFxp
