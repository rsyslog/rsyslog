HOWTO install rsyslog
=====================

*Written by* `Rainer
Gerhards <http://www.adiscon.com/en/people/rainer-gerhards.php>`_

Abstract
--------

**In this paper, I describe how to install** 
`rsyslog <http://www.rsyslog.com/>`_. It is intentionally a brief
step-by-step guide, targeted to those who want to quickly get it up and
running. For more elaborate information, please consult the rest of the
`manual set <manual.html>`_.

How to make your life easier...
-------------------------------

Some folks have thankfully created `RPMs/packages for
rsyslog <rsyslog_packages.html>`_. If you use them, you can spare
yourself many of the steps below. This is highly recommended if there is
a package for your distribution available.

Steps To Do
-----------

Rsyslog does currently only have very limited availability as a package
(if you volunteer to create one, `drop me a
line <mailto:rgerhards@adiscon.com>`_). Thus, this guide focuses on
installing from the source, which thankfully is **quite easy**.

Step 1 - Download Software
~~~~~~~~~~~~~~~~~~~~~~~~~~

For obvious reasons, you need to download rsyslog. Here, I assume that
you use a distribution tarball. If you would like to use a version
directly from the repository, see `build rsyslog from
repository <build_from_repo.html>`_ instead.

Load the most recent build from
`http://www.rsyslog.com/downloads <http://www.rsyslog.com/downloads>`_.
Extract the software with "tar xzf -nameOfDownloadSet-". This will
create a new subdirectory rsyslog-version in the current working
directory. CD into that.

Depending on your system configuration, you also need to install some
build tools, most importantly make, the gcc compiler and the MySQL
development system (if you intend to use MySQL - the package is often
named "mysql-dev"). On many systems, these things should already be
present. If you don't know exactly, simply skip this step for now and
see if nice error messages pop up during the compile process. If they
do, you can still install the missing build environment tools. So this
is nothing that you need to look at very carefully.

Step 2 - Run ./configure
~~~~~~~~~~~~~~~~~~~~~~~~

Run ./configure to adopt rsyslog to your environment. While doing so,
you can also enable options. Configure will display selected options
when it is finished. For example, to enable MySQL support, run

./configure --enable-mysql

Please note that MySQL support by default is NOT disabled.

Step 3 - Compile
~~~~~~~~~~~~~~~~

That is easy. Just type "make" and let the compiler work. On any recent
system, that should be a very quick task, on many systems just a matter
of a few seconds. If an error message comes up, most probably a part of
your build environment is not installed. Check with step 1 in those
cases.

Step 4 - Install
~~~~~~~~~~~~~~~~

Again, that is quite easy. All it takes is a "make install". That will
copy the rsyslogd and the man pages to the relevant directories.

Step 5 - Configure rsyslogd
~~~~~~~~~~~~~~~~~~~~~~~~~~~

In this step, you tell rsyslogd what to do with received messages. If
you are upgrading from stock syslogd, /etc/syslog.conf is probably a
good starting point. Rsyslogd understands stock syslogd syntax, so you
can simply copy over /etc/syslog.conf to /etc/rsyslog.conf. Note since
version 3 rsyslog requires to load plug-in modules to perform useful
work (more about `compatibilty notes v3 <v3compatibility.html>`_). To
load the most common plug-ins, add the following to the top of
rsyslog.conf:

$ModLoad immark # provides --MARK-- message capability
 $ModLoad imudp # provides UDP syslog reception
 $ModLoad imtcp # provides TCP syslog reception and GSS-API (if compiled
to support it)
 $ModLoad imuxsock # provides support for local system logging (e.g. via
logger command)
 $ModLoad imklog # provides kernel logging support (previously done by
rklogd)

Change rsyslog.conf for any further enhancements you would like to see.
For example, you can add database writing as outlined in the paper
"`Writing syslog Data to MySQL <rsyslog_mysql.html>`_\ " (remember you
need to enable MySQL support during step 2 if you want to do that!).

Step 6 - Disable stock syslogd
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

In almost all cases, there already is stock syslogd installed. Because
both it and rsyslogd listen to the same sockets, they can NOT be run
concurrently. So you need to disable the stock syslogd. To do this, you
typically must change your rc.d startup scripts.

For example, under `Debian <http://www.debian.org/>`_ this must be done
as follows: The default runlevel is 2. We modify the init scripts for
runlevel 2 - in practice, you need to do this for all run levels you
will ever use (which probably means all). Under /etc/rc2.d there is a
S10sysklogd script (actually a symlink). Change the name to
\_S10sysklogd (this keeps the symlink in place, but will prevent further
execution - effectively disabling it).

Step 7 - Enable rsyslogd Autostart
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

This step is very close to step 3. Now, we want to enable rsyslogd to
start automatically. The rsyslog package contains a (currently small)
number of startup scripts. They are inside the distro-specific directory
(e.g. debian). If there is nothing for your operating system, you can
simply copy the stock syslogd startup script and make the minor
modifications to run rsyslogd (the samples should be of help if you
intend to do this).

In our Debian example, the actual scripts are stored in /etc/init.d.
Copy the standard script to that location. Then, you need to add a
symlink to it in the respective rc.d directory. In our sample, we modify
rc2.d, and can do this via the command "ln -s ../init.d/rsyslogd
S10rsyslogd". Please note that the S10 prefix tells the system to start
rsyslogd at the same time stock sysklogd was started.

**Important:** if you use the database functionality, you should make
sure that MySQL starts before rsyslogd. If it starts later, you will
receive an error message during each restart (this might be acceptable
to you). To do so, either move MySQL's start order before rsyslogd or
rsyslogd's after MySQL.

Step 8 - Check daily cron scripts
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Most distributions come pre-configured with some daily scripts for log
rotation. As long as you use the same log file names, the log rotation
scripts will probably work quite well. There is one caveat, though. The
scripts need to tell syslogd that the files have been rotated. To do
this, they typically have a part using syslogd's init script to do that.
Obviously, the default scripts do not know about rsyslogd, so they
manipulate syslogd. If that happens, in most cases an additional
instance of stock syslogd is started (in almost all cases, this was not
functional, but it is at least distracting). It also means that rsyslogd
is not properly told about the log rotation, which will lead it to
continue to write to the now-rotated files.

So you need to fix these scripts. See your distro-specific documentation
how they are located. Under most Linuxes, the primary script to modify
is /etc/cron.daily/sysklogd. Watch for a comment "Restart syslogd"
(usually at the very end of the file). The restart command must be
changed to use rsyslogd's rc script.

Also, if you use klogd together with rsyslogd (under most Linuxes you
will do that), you need to make sure that klogd is restarted after
rsyslogd is restarted. So it might be a good idea to put a klogd
reload-or-restart command right after the rsyslogd command in your daily
script. This can save you lots of troubles.

Done
~~~~

This concludes the steps necessary to install rsyslogd. Of course, it is
always a good idea to test everything thoroughly. At a minimalist level,
you should do a reboot and after that check if everything has come up
correctly. Pay attention not only to running processes, but also check
if the log files (or the database) are correctly being populated.

If rsyslogd encounters any serious errors during startup, you should be
able to see them at least on the system console. They might not be in
log file, as errors might occur before the log file rules are in place.
So it is always a good idea to check system console output when things
don't go smooth. In some rare cases, enabling debug logging (-d option)
in rsyslogd can be helpful. If all fails, go to
`www.rsyslog.com <http://www.rsyslog.com>`_ and check the forum or
mailing list for help with your issue.

Housekeeping stuff
------------------

This section and its subsections contain all these nice things that you
usually need to read only if you are really curios ;)

Feedback requested
~~~~~~~~~~~~~~~~~~

I would appreciate feedback on this tutorial. It is still in its
infancy, so additional ideas, comments or bug sighting reports are very
welcome. Please `let me know <mailto:rgerhards@adiscon.com>`_ about
them.

Revision History
~~~~~~~~~~~~~~~~

-  2005-08-08 \* `Rainer
   Gerhards <http://www.adiscon.com/en/people/rainer-gerhards.php>`_ \*
   Initial version created
-  2005-08-09 \* `Rainer
   Gerhards <http://www.adiscon.com/en/people/rainer-gerhards.php>`_ \*
   updated to include distro-specific directories, which are now
   mandatory
-  2005-09-06 \* `Rainer
   Gerhards <http://www.adiscon.com/en/people/rainer-gerhards.php>`_ \*
   added information on log rotation scripts
-  2007-07-13 \* `Rainer
   Gerhards <http://www.adiscon.com/en/people/rainer-gerhards.php>`_  \*
   updated to new autotools-based build system
-  2008-10-01 \* `Rainer
   Gerhards <http://www.adiscon.com/en/people/rainer-gerhards.php>`_  \*
   added info on building from source repository

Copyright
~~~~~~~~~

Copyright © 2005-2008 `Rainer
Gerhards <http://www.adiscon.com/en/people/rainer-gerhards.php>`_ and
`Adiscon <http://www.adiscon.com/en/>`_.

Permission is granted to copy, distribute and/or modify this document
under the terms of the GNU Free Documentation License, Version 1.2 or
any later version published by the Free Software Foundation; with no
Invariant Sections, no Front-Cover Texts, and no Back-Cover Texts. A
copy of the license can be viewed at
`http://www.gnu.org/copyleft/fdl.html <http://www.gnu.org/copyleft/fdl.html>`_.

[`manual index <manual.html>`_\ ] [`rsyslog
site <http://www.rsyslog.com/>`_\ ]

This documentation is part of the `rsyslog <http://www.rsyslog.com/>`_
project.

Copyright © 2008 by `Rainer Gerhards <http://www.gerhards.net/rainer>`_
and `Adiscon <http://www.adiscon.com/>`_. Released under the GNU GPL
version 1.2 or higher.
