troubleshooting problems
========================

**Having trouble with** `rsyslog <https://www.rsyslog.com>`_? This page
provides some tips on where to look for help and what to do if you need
to ask for assistance. This page is continuously being expanded.

Useful troubleshooting resources are:

-  The `rsyslog documentation <https://www.rsyslog.com/doc>`_ - note that
   the online version always covers the most recent development version.
   However, there is a version-specific doc set in each tarball. If you
   installed rsyslog from a package, there usually is a rsyslog-doc
   package, that often needs to be installed separately.

-  Check the `rsyslog github issue tracker <https://github.com/rsyslog/rsyslog/issues>`_ and 
   `the bugzilla <http://bugzilla.adiscon.com>`_ to see if your
   problem is a known (and even fixed ;)) bug.
   **Note:** the preferred way to create new bugs is via github.
   The bugzilla does no longer accept new bugs. It is just kept
   to work on old ones and as a reference source for ChangeLog entries.

Malformed Messages and Message Properties
-----------------------------------------

A common trouble source are `ill-formed syslog
messages <syslog_parsing.html>`_, which lead to all sorts of
interesting problems, including malformed hostnames and dates. Read the
quoted guide to find relief. A common symptom is that the %HOSTNAME%
property is used for generating dynafile names, but some gibberish
shows up. This is caused by the malformed syslog messages, so be sure to
read the :doc:`guide on syslog parsing <../whitepapers/syslog_parsing>`
if you face that problem. Just
let me add that the common work-around is to use %FROMHOST% or
%FROMHOST-IP% instead. These do not take the hostname from the message,
but rather use the host that sent the message (taken from the socket
layer). Of course, this does not work over NAT or relay chains, where
the only cure is to make sure senders emit well-formed messages.

Configuration Problems
----------------------

Rsyslog has support for
configuration checking. It offers a special command line switch (-N<*value*>)
that puts it into "config verification mode". In that mode, it interprets
and checks the configuration file, but does not startup. This mode can be
used in parallel to a running instance of rsyslogd.

Please note that many distros have (unfortunately) begun to split the rsyslog
config into many small config snippets (a.k.a input files), usually in `/etc/rsyslog.d`. From
rsyslog's point of view, only a single config file exists and these snippets
are logically combined into that single config. For that reason, config checking
does usually **not** work on snippets. Because the interdependencies are missing.
For example, binding a ruleset to a module that is not possible if the ruleset is
loaded outside of the snippet.

**As a general guideline, never do config checks on config snippets. Always use
the main configuration file as a starting point** (usually  /etc/rsyslog.conf`).

The *value* given after -N is a set of binary values. Currently, there only is

======= ======================================
value   meaning
1       turn on config checking
2       permit checking of include files
======= ======================================

Where 2 automatically turns on config checking mode, if not given. In that
sense ``-N2`` and ``-N3`` are equivalent.

Values other than given in the table above are **not** supported and may lead
to unpredictable results.

When set to check include files, some conditions are relaxed. For example,
rsyslog usually requires that at least one action is defined somewhere in
the configuration. For obvious reasons, it would not make much sense to run
an instance without any action. However, when an include file is checked,
it may happen that it contains no actions as all. As such, the requirement
to include one action has been lifted in include file checking.

To check a full rsyslog configuration, run rsyslog interactively as follows:

::

 $ /path/to/rsyslogd -f/path/to/config-file -N1

You should also specify other options you usually give.
Any problems experienced are reported to stderr [aka
"your screen" (if not redirected)].

If you would like to check just an include file, instead use:

::

 $ /path/to/rsyslogd -f/path/to/config-file -N3

Sometimes problems are rooted in config include files, and especially the
order in which they are processed. To troubleshoot these kinds of issues, you
can use the rsyslogd `-o` option: it permits to specify a file that shall
receive a full copy of rsyslog's current configuration **as rsyslog sees it**.
This means all include file content is directly inside that file at
exactly the spot where rsyslog sees it. The output file is almost a
verbatim copy of the original full rsyslog config. For troubleshooting
purposes it additionally contains comment lines that indicate where
content from specific include files begins and ends. The include file
is correctly named in these comments.

This option can be used together with `-N`. Again, it is best to run
rsyslog interactively. Do as such::

 $ /path/to/rsyslogd -f/path/to/config-file -N3 -o /path/to/full-conf-file


Checking Connection Problems
----------------------------

If a client cannot connect via the network to the rsyslog server, you
can do a connection check via netcat. That will verify if the sender
is able to deliver to an application running on the receiver. Netcat
is a very simple receiver, so we can be sure that no netcat problem
will interfere with this test.

With netcat, you can test UDP and TCP syslog connections, but not TLS.

To do this test, you need to

* on the client

  - stop the syslog sender process, if possible. If the sender is 
    rsyslog, you can use the same procedure described below for the
    server.

* on the rsyslog server

  - stop and/or disable rsyslog
    On systemd systems (newer distro versions), systemd might
    automatically restart rsyslog when data is written to the system
    log socket. To be sure, we recommend to disable the service on
    those systems. This sequence should work:
    $ systemctl disable rsyslog.service
    $ systemctl stop rsyslog.service

  - open a terminal session, and start a netcat listener **on the same
    listening port** that you have configured inside rsyslog. Note that
    if you use a privileged port, you need to execute nc as root.
    We assume port 13515 is used for rsyslog, so do this:
    $ nc -k -l <ip-of-server> 13515  # [FOR TCP] OR sudo nc ...
    $ nc -u -l <ip-of-server> 13515  # [FOR UDP] OR sudo nc ...

* on the syslog client

  - send a test message via netcat:
    $ echo "test message 1" | nc <ip-of-server> 13515 # [FOR TCP]
    $ echo "test message 1" | nc <ip-of-server> 13515 # [FOR UDP]

* on the server

  - check if you received the test message. Note that you might also
    have received additional messages if the original sender process
    was not stopped. If you see garbage, most probably some sender
    tries to send via TLS.
  - you can stop nc by <ctl>-c

If you did not see the test message arrive at the central server,
the problem is most probably rooted in the network configuration
or other parts of the system configuration. Things to check are
- firewall settings

- for UDP: does the sender have a route back to the original sender?
  This is often required by modern systems to prevent spoofing; if the
  sender cannot be reached, UDP messages are discarded AFTER they have
  been received by the OS (an app like netcat or rsyslog will never
  see them)

- if that doesn't help, use a network monitor (or tcpdump, Wireshark, ...)
  to verify that the network packet at least reaches the system.

If you saw the test message arrive at the central server, the problem
most probably is related to the rsyslog configuration or the system
configuration that affects rsyslog (SELinux, AppArmor, ...).

A good next test is to run rsyslog interactively, just like you did
with netcat:

* on the server
  - make sure the rsyslog service is still stopped

  - run
    $ sudo /usr/sbin/rsyslogd -n

* on the client

  - send a test message

* on the server
  - check if the message arrived

  - terminate rsyslog by pressing <ctl>-c

If the test message arrived, you definitely have a problem with the
system configuration, most probably in SELinux, AppArmor or a similar
subsystem. Note that your interactive security context is quite different
from the rsyslog system service context.

If the test message did not arrive, it is time to generate a debug
log to see exactly what rsyslog does. A full description is in this file
a bit down below, but in essence you need to do

* on the server
  - make sure the rsyslog service is still stopped
  - run

    $ sudo /usr/sbin/rsyslogd -nd 2> rsyslog-debug.log

* on the client
  - send a test message

* on the server
  - stop rsyslog by pressing <ctl>-
  - review debug log
   

Asking for Help
---------------

If you can't find the answer yourself, you should look at these places
for community help.

-  The `rsyslog mailing
   list <http://lists.adiscon.net/mailman/listinfo/rsyslog>`_. This is a
   low-volume list which occasional gets traffic spikes. The mailing
   list is probably a good place for complex questions.
   This is the preferred method of obtaining support.
-  The `rsyslog forum <http://kb.monitorware.com/rsyslog-f40.html>`_.

Debug Log
---------

If you ask for help, there are chances that we need to ask for an
rsyslog debug log. The debug log is a detailed report of what rsyslog
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
when you start rsyslogd as a background daemon. Let's assume "-c5" is
the only option used. Then, do the following:

-  make sure rsyslogd as a daemon is stopped (verify with ps -ef\|grep
   rsyslogd)
-  make sure you have a console session with root permissions
-  run rsyslogd interactively: ```/sbin/rsyslogd ..your options.. -dn >
   logfile```
   where "your options" is what you usually use. /sbin/rsyslogd is the
   full path to the rsyslogd binary (location different depending on
   distro). In our case, the command would be
   ```/sbin/rsyslogd -c5 -dn > logfile```
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
contract <http://www.rsyslog.com/professional-services/>`_. Free support comes without any
guarantees, include no guarantee on confidentiality [aka "we don't want
to be sued for work were are not even paid for ;)]. **So if you submit
debug logs, do so at your sole risk**. By submitting them, you accept
this policy.

Segmentation Faults
-------------------

Rsyslog has a very rapid development process, complex capabilities and
now gradually gets more and more exposure. While we are happy about
this, it also has some bad effects: some deployment scenarios have
probably never been tested and it may be impossible to test them for the
development team because of resources needed. So while we try to avoid
this, you may see a serious problem during deployments in demanding,
non-standard, environments (hopefully not with a stable version, but
chances are good you'll run into troubles with the development
versions).

In order to aid the debugging process, it is useful to have debug symbols
on the system. If you build rsyslog yourself, make sure that the ``-g``
option is included in CFLAGS. If you use packages, the debug symbols come
in their own package. **It is highly recommended to install that package
as it provides tremendous extra benefit.** 

For RPM-based systems like CentOS or RHEL, you can install the
debuginfo package by running:

::

  yum install rsyslog-debuginfo 

For Debian-based systems like Ubuntu, you will need to edit the apt sources
list to include the debug symbols repository by appending ``main/debug``.
For our launchpad PPA, you can add the following line to your launchpad sources list,
for example in ``/etc/apt/sources.list.d/adiscon-ubuntu-v8-stable-jammy.list``:

::

    deb https://ppa.launchpadcontent.net/adiscon/experimental/ubuntu/ jammy main main/debug
  
Every package will have its own debug symbols package, for example for rsyslog-openssl
there is a rsyslog-openssl-dbgsym package. To install the debug symbols for the main
rsyslog package you can run:

::

  apt-get update
  apt-get install rsyslog-dbgsym


Active support from the user base is very important to help us track
down those things. Most often, serious problems are the result of some
memory misaddressing. During development, we routinely use valgrind, a
very well and capable memory debugger. This helps us to create pretty
clean code. But valgrind can not detect everything, most importantly not
code paths that are never executed. So of most use for us is
information about aborts and abort locations.

Unfortunately, faults rooted in addressing errors typically show up only
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
for example, that means you need to have a "ulimit -c unlimited" in
place.

Now let's assume you got a core file (e.g. in /core.1234). So what to do
next? Sending a core file to us is most often pointless - we need to
have the exact same system configuration in order to interpret it
correctly. Obviously, chances are extremely slim for this to be. So we
would appreciate if you could extract the most important information.
This is done as follows:

::

   $ gdb /path/to/rsyslogd
   $ core /core.1234
   $ info thread
   $ thread apply all bt full
   $ q # quits gdb

The same method can be applied to a running rsyslog process that suffers
from a lock condition. E.g. if you experience that rsyslog is no longer
forwarding log messages, but this cannot be reproduced in our lab. Using 
gdb to review the state of the active threads may be an option to see 
which thread is causing the problem (e.g. by locking itself or being in a
wait state).

Again, basically the same steps can be applied. But, instead of using a 
core file, we will require the currently used PID. So make sure to acquire
the PID before executing gdb.

::

   $ gdb /path/to/rsyslogd
   $ attach PID # numerical value
   $ info thread
   $ thread apply all bt full
   $ q # quits gdb

Then please send all information that gdb spit out to the development
team. It is best to first ask on the forum or mailing list on how to do
that. The developers will keep in contact with you and, I fear, will
probably ask for other things as well ;)


Note that we strive for highest reliability of the engine even in
unusual deployment scenarios. Unfortunately, this is hard to achieve,
especially with limited resources. So we are depending on cooperation
from users. This is your chance to make a big contribution to the
project without the need to program or do anything else except get a
problem solved.
