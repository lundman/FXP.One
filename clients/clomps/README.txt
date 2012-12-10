
l-compare-sites = cLompS


The idea with clomps is to connect to multiple FTP sites, fetch
directory listings, and compare. 

The eventual outcome should be:


1)

  Listing all new entries since last time you ran clomps. (Total new,
  not printing duplicates). 


2)

  Compute the matrix of the new items, and all sites and display where
  the new items are Missing, Incomplete, Skipped or available. This is
  for synchronisation purposes, so you can easily see where items are
  available, and where they need to be sent.


3)

  Eventually, save out queue files to process, and synchronise the
  items in 2).


Naturally, the skiplists should be consulted for each individual site
so it knows to ignore a new items for that particular site.

An empty window is the same as "OK" the item is there. I didn't want
to populate "OK"s as I wanted sites that were missing items to stand
out more.



          Sites -=>                       One   Two   Three Four  Five 
New Items--------------------------------|-----|-----|-----|-----|DOWN-|-----|
d NetBSD-2.0.2                           |     |INC  |     |skip |  
d OpenSolaris-1.0.4                      |MISS |MISS |MISS |     |  
d Latest.Item.Of.Something-GRP           |     |     |     |     |  
f another.file.item.rar                  |INC  |skip |skip |INC  |  
d Serenity.Trailer                       |MISS |skip |     |MISS |  






As a client, the source code is fairly poor. I put everything into one
C file. I was more interested in writing something that would use
FXP.One as fast as possible. As it happens, clomps took roughly 5
hours to implement.  Could do with some cleaning up in terms of OOPS
style. 


Auto-trading config syntax possibilities:

AUTO | PASSNUM=1 | SRC=site1 | DST=site2 | ACCEPT=*patterns* ... |
       REJECT=*patterns* ...

The pass-number is used to control order. It will start all auto-trade
lines with passnum=1 first. Once they are finished, it will run
passnum=2. This way you can control the entire flow of things. For
example:

passnum=1
a->b
e->f

passnum=2
b->c
a->f

Would move items from a -> b -> c and ensure a is only used once at
any time.

AUTO is parsed after the new entries check have run, and do only match
based on those items.
 
There has to be an ACCEPT field, even if it is just "*" for
everything. REJECT is optional. Both fields are in the form of
"pattern/pattern/pattern/...". 

For example:

"*simpsons*/*futurama/*firefly*"


NOTE! Auto queueing can handle source site having multiple paths, but
will ONLY USE THE FIRST DIRECTORY SPECIFIED IN THE DESTINATION
SITE. For obvious reasons, clomps can not know how to sort items. You
would have to do that by using multiple AUTOQ lines.



TODO:

FEAT: If we successfully did move something, add option to
automatically retry the same settings again (incase it was not
finished). Also, return a different returncode based on if we did
something or not.
