
These are the samples that I have included for the lion library.



These samples are small and quick, and are mostly included to
high-light a particular feature of lion. Since the user_data isn't
being used it a particularly smart manner, they probably do not server
as good basis for your own application, unless they are small.


relay	   : Simple network only example to relay/port-forward.
		   : Open listening socket, and for any new connection, issues a
		   : remote host connection and once established, relays all
		   : data.


send-file  : Simple listen socket, sending a file on connection.
		   : Open listening socket, on connections, open a file for
		   : reading and sends to the socket. 


get-file   : Simple listen socket, and write-file example.
		   : Open a listening socket, on connections, open file for
		   : writing and for all input from socket, write to file.
		   : No locking, so multiple connections would all write to
		   : same file, but as TRUNC is used, only last connection stays.

		 
fork	   : Example of fork.
		   : forks off a child, passes messages between parent and
		   : child until parent asks child to exit. Also shows how to
		   : wait for the exit code.
		  

system     : Executes a program, receiving information.
		   : Uses lion_execv() to start /bin/sh, and sends it "who"
		   : command which it prints to console.
		  

system-send: Connects to remote host, spawns child and sends data to remote.
		   : Connects to remote host, and upon connection spawns "cat
		   : /bin/sh" which it sends to remote host. Can be used with
		   : get-file example. Demonstrates lion_system().


ssl-connect: Connects to remote host and attempts SSL authentication.
		   : Simple example attempting to show the client side SSL.


ssl-listen : Listens for new connections and auto senses SSL.
		   : Opens a listening socket and attempts to use SSL.


getdirs    : Sample that uses contrib/libdirlist. 
		   : Spawns off dirlist helpers, and opens a listening socket.
		   : You can then issue "ls" request with all supported switches.




These are larger samples, and are done properly. That is to say, they
use user_data to point to your own structure node that has the handle
and other useful information (such as type for example). These
examples are probably a better starting point for any larger applications.


template   : Simple chat server system.
		   : Open a listening socket, lets user "register" with a nick
		   : and relays all text to the others in a simple chat system
		   : way. Shows how you can have different types passed to
		   : user_data and events acted upon differently based on
		   : that. Also has simple parsing and iteration, like the
		   : "WHO" command that iterates all nodes and send
		   : information to a particular node. And the opposite, the
		   : say_all function that iterates all, and send text to each one.
