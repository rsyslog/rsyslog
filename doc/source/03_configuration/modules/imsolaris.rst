*******************************
imsolaris: Solaris Input Module
*******************************

===========================  ===========================================================================
**Module Name:**             **imsolaris**
**Author:**                  `Rainer Gerhards <https://rainer.gerhards.net/>`_ <rgerhards@adiscon.com>
===========================  ===========================================================================


Purpose
=======

Reads local Solaris log messages including the kernel log.

This module is specifically tailored for Solaris. Under Solaris, there
is no special kernel input device. Instead, both kernel messages as well
as messages emitted via syslog() are received from a single source.

This module obeys the Solaris door() mechanism to detect a running
syslogd instance. As such, only one can be active at one time. If it
detects another active instance at startup, the module disables itself,
but rsyslog will continue to run.


Configuration Parameters
========================

.. note::

   Parameter names are case-insensitive.


|FmtObsoleteName| Directives
----------------------------

| functions:: $IMSolarisLogSocketName <name>

   This is the name of the log socket (stream) to read. If not given,
   /dev/log is read.


Caveats/Known Bugs
==================

None currently known. For obvious reasons, works on Solaris, only (and
compilation will most probably fail on any other platform).


Examples
========

The following sample pulls messages from the default log source

.. code-block:: none

   $ModLoad imsolaris


