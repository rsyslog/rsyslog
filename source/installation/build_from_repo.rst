Building rsyslog from the source repository
===========================================

Building from the repsitory is not much different than building from the
source tarball, but some files are missing because they are output files
and thus do not belong into the repository.

Obtaining the Source
--------------------

First of all, you need to download the sources. Rsyslog is currently
kept in a git repository. You can clone this repository either via http
or git protocol (with the later being much faster. URLS are:

-  git://git.adiscon.com/git/rsyslog.git
-  http://git.adiscon.com/git/rsyslog.git

There is also a browsable version (gitweb) available at
`http://git.adiscon.com/?p=rsyslog.git;a=summary <http://git.adiscon.com/?p=rsyslog.git;a=summary>`_.
This version also offers snapshots of each commit for easy download. You
can use these if you do not have git present on your system.

After you have cloned the repository, you are in the master branch by
default. This is where we keep the devel branch. If you need any other
branch, you need to do a "git checkout --track -b branch origin/branch".
For example, the command to check out the beta branch is "git checkout
--track -b beta origin/beta".

Prequisites
-----------

To build the compilation system, you need the **pkg-config** package (an
utility for autotools) present on your system. Otherwise, configure will
fail with something like

::

    checking for SYSLOG_UNIXAF support... yes
    checking for FSSTND support... yes
    ./configure: line 25895: syntax error near unexpected token `RELP,'
    ./configure: line 25895: ` PKG_CHECK_MODULES(RELP, relp >= 0.1.1)'

Creating the Build Environment
------------------------------

This is fairly easy: just issue "**autoreconf -fvi**\ ", which should do
everything you need. Once this is done, you can follow the usual
./configure steps just like when you downloaded an official distribution
tarball (see the `rsyslog install guide <install.html>`_, starting at
step 2, for further details about that).

Special Compile-Time Options
----------------------------

On some platforms, compile-time issues occur, like the one shown below:

::

    make[2]: Entering directory `/home/az/RSyslog/rsyslog-5.5.0/tools'
      CCLD   rsyslogd
    rsyslogd-omfile.o: In function `getClockFileAccess':
    /home/az/RSyslog/rsyslog-5.5.0/tools/omfile.c:91: undefined reference to `__sync_fetch_and_add_8'
    /home/az/RSyslog/rsyslog-5.5.0/tools/omfile.c:91: undefined reference to `__sync_fetch_and_add_8'
    /home/az/RSyslog/rsyslog-5.5.0/tools/omfile.c:91: undefined reference to `__sync_fetch_and_add_8'

Note that the exact error messages can be different. These type of
errors stem down to atomic instruction support in GCC, which is somewhat
depending on the machine architecture it compiles code for. Very old
machines (like the original i386) do not even at all provide support for
these instructions.

The availability of atomic instructions is vital for rsyslog - it can
not be built without them. Consequently, there is a configure check
included for them. But under some circumstances, GCC seems to report
they are available, but does not provide implementations for all of them
(at least this is my observation...). The simple cure is to make sure
that GCC generates code for a modern-enough architecture. This, for
example, can be done as follows:

::

    ./configure CFLAGS="-march=i586 -mcpu=i686" --enable-imfile ... (whatever you need)

These settings should resolve the issue .

[`manual index <manual.html>`_\ ] [`rsyslog
site <http://www.rsyslog.com/>`_\ ]

This documentation is part of the `rsyslog <http://www.rsyslog.com/>`_
project.
Copyright © 2008, 2009 by `Rainer
Gerhards <http://www.gerhards.net/rainer>`_ and
`Adiscon <http://www.adiscon.com/>`_. 
Released under the GNU GPL version 3 or higher.
