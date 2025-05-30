What?
=====
This directory contains *external* plugins. That are plugins which run
in an external process, contrary to the in-process plugins that rsyslog
traditionally uses.

Why?
====
The prime use case probably is to enable plugin development in languages
other than C. Often, the high performance of a pure C plugin is not needed,
simply because the use case does not need it or the destination that is
being connected to is itself "slow enough" that one does not note
the difference.

Something missing?
==================
Consider writing it in your favorite language and contributing it back
to the rsyslog project. Patches are happily accepted. There is a
"skeletons" directory which contains simple skeletons which simply 
can be amended by your app-specific coding. The number of languages
for skeletons is ever increasing. If there is not yet a skeleton for
your favorite languagem, we will gladly help you creating one. It's
really easy and usually done with just a couple of code lines!

How to write your own plugin?
=============================
View this presentation to learn howto write a plugin in 2 minutes:
    http://www.slideshare.net/rainergerhards1/writing-rsyslog-p

See the [INTERFACE](INTERFACE.md) file. And see the [skeletons](skeletons)
subdirectory for existing ready-to-copy sample plugins that do the
necessary plumbing.

Evolving this Interface
=======================
The interface is build via omprog, and is currently being evolved. What
is there is robust and stable, but even better stuff is upcoming.
