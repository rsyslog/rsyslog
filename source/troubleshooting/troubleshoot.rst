troubleshooting rsyslog
-----------------------

**Having trouble with** `rsyslog <http://www.rsyslog.com>`_? This page
provides some tips on where to look for help and what to do if you need
to ask for assistance. This page is continously being expanded.

Useful troubleshooting ressources are:

-  The `rsyslog documentation <http://www.rsyslog.com/doc>`_ - note that
   the online version always covers the most recent development version.
   However, there is a version-specific doc set in each tarball. If you
   installed rsyslog from a package, there usually is a rsyslog-doc
   package, that often needs to be installed separately.
-  The `rsyslog wiki <http://wiki.rsyslog.com>`_ provides user tips and
   experiences.
-  Check `the bugzilla <http://bugzilla.adiscon.com>`_ to see if your
   problem is a known (and even fixed ;)) bug.

**Malformed Messages and Message Properties**

A common trouble source are `ill-formed syslog
messages <syslog_parsing.html>`_, which lead to to all sorts of
interesting problems, including malformed hostnames and dates. Read the
quoted guide to find relief. A common symptom is that the %HOSTNAME%
property is used for generating dynafile names, but some glibberish
shows up. This is caused by the malformed syslog messages, so be sure to
read the `guide <syslog_parsing.html>`_ if you face that problem. Just
let me add that the common work-around is to use %FROMHOST% or
%FROMHOST-IP% instead. These do not take the hostname from the message,
but rather use the host that sent the message (taken from the socket
layer). Of course, this does not work over NAT or relay chains, where
the only cure is to make sure senders emit well-formed messages.

**Configuration Problems**

Rsyslog 3.21.1 and above has been enhanced to support extended
configuration checking. It offers a special command line switch (-N1)
that puts it into "config verfication mode". In that mode, it interprets
and check the configuration file, but does not startup. This mode can be
used in parallel to a running instance of rsyslogd.

To enable it, run rsyslog interactively as follows:

***/path/to/rsyslogd -f/path/to/config-file -N1***

You should also specify other options you usually give (like -c3 and
whatever else). Any problems experienced are reported to stderr [aka
"your screen" (if not redirected)].

**Configuration Graphs**

Starting with rsyslog 4.3.1, the
"`$GenerateConfigGraph <rsconf1_generateconfiggraph.html>`_\ " command
is supported, a very valuable troubleshooting tool. It permits to
generate a graph of how rsyslogd understood its configuration file. It
is assumed that many configuration issues can easily be detected just by
looking at the configuration graph. Full details of how to generate the
graphs, and what to look for can be found in the
"`$GenerateConfigGraph <rsconf1_generateconfiggraph.html>`_\ " manual
page.

**Asking for Help**

If you can't find the answer yourself, you should look at these places
for community help.

-  The `rsyslog forum <http://kb.monitorware.com/rsyslog-f40.html>`_.
   This is the preferred method of obtaining support.
-  The `rsyslog mailing
   list <http://lists.adiscon.net/mailman/listinfo/rsyslog>`_. This is a
   low-volume list which occasional gets traffic spikes. The mailing
   list is probably a good place for complex questions.

**Debug Log**

If you ask for help, there are chances that we need to ask for an
rsyslog debug log. The debug log is a detailled report of what rsyslog
does during processing. As such, it may even be useful for your very own
troubleshooting. People have seen things inside their debug log that
enabled them to find problems they did not see before. So having a look
at the debug log, even before asking for help, may be useful.

Note that the debug log contains most of those things we consider
useful. This is a lot of information, but may still be too few. So it
sometimes may happen that you will be asked to run a specific version
which has additional debug output. Also, we revise from time to time
what is worth putting into the standard debug log. As such, log content
may change from version to version. We do not guarantee any specific
debug log contents, so do not rely on that. The amount of debug logging
can also be controlled via some environment options. Please see
`debugging support <debug.html>`_ for further details.

In general, it is advisable to run rsyslogd in the foreground to obtain
the log. To do so, make sure you know which options are usually used
when you start rsyslogd as a background daemon. Let's assume "-c3" is
the only option used. Then, do the following:

-  make sure rsyslogd as a daemon is stopped (verify with ps -ef\|grep
   rsyslogd)
-  make sure you have a console session with root permissions
-  run rsyslogd interactively: /sbin/rsyslogd ..your options.. -dn >
   logfile
   where "your options" is what you usually use. /sbin/rsyslogd is the
   full path to the rsyslogd binary (location different depending on
   distro). In our case, the command would be
   /sbin/rsyslogd -c3 -dn > logfile
-  press ctrl-C when you have sufficient data (e.g. a device logged a
   record)
   **NOTE: rsyslogd will NOT stop automatically - you need to ctrl-c out
   of it!**
-  Once you have done all that, you can review logfile. It contains the
   debug output.
-  When you are done, make sure you re-enable (and start) the background
   daemon!

If you need to submit the logfile, you may want to check if it contains
any passwords or other sensitive data. If it does, you can change it to
some **consistent** meaningless value. **Do not delete the lines**, as
this renders the debug log unusable (and makes Rainer quite angry for
wasted time, aka significantly reduces the chance he will remain
motivated to look at your problem ;)). For the same reason, make sure
whatever you change is change consistently. Really!

Debug log file can get quite large. Before submitting them, it is a good
idea to zip them. Rainer has handled files of around 1 to 2 GB. If
your's is larger ask before submitting. Often, it is sufficient to
submit the first 2,000 lines of the log file and around another 1,000
around the area where you see a problem. Also, ask you can submit a file
via private mail. Private mail is usually a good way to go for large
files or files with sensitive content. However, do NOT send anything
sensitive that you do not want the outside to be known. While Rainer so
far made effort no to leak any sensitive information, there is no
guarantee that doesn't happen. If you need a guarantee, you are probably
a candidate for a `commercial support
contract <professional_support.html>`_. Free support comes without any
guarantees, include no guarantee on confidentiality [aka "we don't want
to be sued for work were are not even paid for ;)]. **So if you submit
debug logs, do so at your sole risk**. By submitting them, you accept
this policy.

**Segmentation Faults**

Rsyslog has a very rapid development process, complex capabilities and
now gradually gets more and more exposure. While we are happy about
this, it also has some bad effects: some deployment scenarios have
probably never been tested and it may be impossible to test them for the
development team because of resources needed. So while we try to avoid
this, you may see a serious problem during deployments in demanding,
non-standard, environments (hopefully not with a stable version, but
chances are good you'll run into troubles with the development
versions).

Active support from the user base is very important to help us track
down those things. Most often, serious problems are the result of some
memory misadressing. During development, we routinely use valgrind, a
very well and capable memory debugger. This helps us to create pretty
clean code. But valgrind can not detect everything, most importantly not
code pathes that are never executed. So of most use for us is
information about aborts and abort locations.

Unforutnately, faults rooted in adressing errors typically show up only
later, so the actual abort location is in an unrelated spot. To help
track down the original spot, `libc later than 5.4.23 offers
support <http://www.gnu.org/software/hello/manual/libc/Heap-Consistency-Checking.html>`_
for finding, and possible temporary relief from it, by means of the
MALLOC\_CHECK\_ environment variable. Setting it to 2 is a useful
troubleshooting aid for us. It will make the program abort as soon as
the check routines detect anything suspicious (unfortunately, this may
still not be the root cause, but hopefully closer to it). Setting it to
0 may even make some problems disappear (but it will NOT fix them!).
With functionality comes cost, and so exporting MALLOC\_CHECK\_ without
need comes at a performance penalty. However, we strongly recommend
adding this instrumentation to your test environment should you see any
serious problems. Chances are good it will help us interpret a dump
better, and thus be able to quicker craft a fix.

In order to get useful information, we need some backtrace of the abort.
First, you need to make sure that a core file is created. Under Fedora,
for example, that means you need to have an "ulimit -c unlimited" in
place.

Now let's assume you got a core file (e.g. in /core.1234). So what to do
next? Sending a core file to us is most often pointless - we need to
have the exact same system configuration in order to interpret it
correctly. Obviously, chances are extremely slim for this to be. So we
would appreciate if you could extract the most important information.
This is done as follows:

-  $gdb /path/to/rsyslogd
-  $info thread
-  you'll see a number of threads (in the range 0 to n with n being the
   highest number). For **each** of them, do the following (let's assume
   that i is the thread number):

   -  $ thread i (e.g. thread 0, thread 1, ...)
   -  $bt

-  then you can quit gdb with "$q"

Then please send all information that gdb spit out to the development
team. It is best to first ask on the forum or mailing list on how to do
that. The developers will keep in contact with you and, I fear, will
probably ask for other things as well ;)

Note that we strive for highest reliability of the engine even in
unusual deployment scenarios. Unfortunately, this is hard to achieve,
especially with limited resources. So we are depending on cooperation
from users. This is your chance to make a big contribution to the
project without the need to program or do anything else except get a
problem solved ;)

[`manual index <manual.html>`_\ ] [`rsyslog
site <http://www.rsyslog.com/>`_\ ]

This documentation is part of the `rsyslog <http://www.rsyslog.com/>`_
project.
Copyright © 2008-2010 by `Rainer
Gerhards <http://www.gerhards.net/rainer>`_ and
`Adiscon <http://www.adiscon.com/>`_. Released under the GNU GPL version
3 or higher.
