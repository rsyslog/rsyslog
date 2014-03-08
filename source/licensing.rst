Licensing
=========

If you intend to use rsyslog inside a GPLv3 compatible project, you are free to
do so. You don't even need to continue reading. If you intend to use rsyslog
inside a non-GPLv3 compatible project, rsyslog offers you some liberties to do
that, too. However, you then need to study the licensing details in depth.

The project hopes this is a good compromise, which also gives a boost to
fellow free software developers who release under GPLv3.

And now on to the dirty and boring license details, still on a executive
summary level. For the real details, check source files and the files
COPYING and COPYING.LESSER inside the distribution.

The rsyslog package contains several components:

-  the rsyslog core programs (like rsyslogd)
-  plugins (like imklog, omrelp, ...)
-  the rsyslog runtime library

Each of these components can be thought of as individual projects. In
fact, some of the plugins have different main authors than the rest of
the rsyslog package. All of these components are currently put together
into a single "rsyslog" package (tarball) for convenience: this makes it
easier to distribute a consistent version where everything is included
(and in the right versions) to build a full system. Platform package
maintainers in general take the overall package and split off the
individual components, so that users can install only what they need. In
source installations, this can be done via the proper ./configure
switches.

However, while it is convenient to package all parts in a single
tarball, it does not imply all of them are necessarily covered by the
same license. Traditionally, GPL licenses are used for rsyslog, because
the project would like to provide free software. GPLv3 has been used
since around 2008 to help fight for our freedom. All rsyslog core
programs are released under GPLv3. But, from the beginning on, plugins
were separate projects and we did not impose and license restrictions on
them. So even though all plugins that currently ship with the rsyslog
package are also placed under GPLv3, this can not taken for granted. You
need to check each plugins license terms if in question - this is
especially important for plugins that do NOT ship as part of the rsyslog
tarball.

In order to make rsyslog technology available to a broader range of
applications, the rsyslog runtime is, at least partly, licensed under
LGPL. If in doubt, check the source file licensing comments. As of now,
the following files are licensed under LGPL:

-  queue.c/.h
-  wti.c/.h
-  wtp.c/.h
-  vm.c/.h
-  vmop.c/.h
-  vmprg.c/.h
-  vmstk.c/.h
-  expr.c/.h
-  sysvar.c/.h
-  ctok.c/.h
-  ctok\_token.c/.h
-  regexp.c/.h
-  sync.c/.h
-  stream.c/.h
-  var.c/.h

This list will change as time of the runtime modularization. At some
point in the future, there will be a well-designed set of files inside a
runtime library branch and all of these will be LGPL. Some select extras
will probably still be covered by GPL. We are following a similar
licensing model in GnuTLS, which makes effort to reserve some
functionality exclusively to open source projects.
