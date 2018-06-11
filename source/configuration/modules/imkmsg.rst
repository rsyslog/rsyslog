**********************************
imkmsg: /dev/kmsg Log Input Module
**********************************

===========================  ===========================================================================
**Module Name:**             **imkmsg**
**Author:**                  `Rainer Gerhards <https://rainer.gerhards.net/>`_ <rgerhards@adiscon.com>
                             Milan Bartos <mbartos@redhat.com>
===========================  ===========================================================================


Purpose
=======

Reads messages from the /dev/kmsg structured kernel log and submits them
to the syslog engine.

The printk log buffer contains log records. These records are exported
by /dev/kmsg device as structured data in the following format:
"level,sequnum,timestamp;<message text>\\n"
There could be continuation lines starting with space that contains
key/value pairs.
Log messages are parsed as necessary into rsyslog msg\_t structure.
Continuation lines are parsed as json key/value pairs and added into
rsyslog's message json representation.


Configuration Parameters
========================

.. note::

   Parameter names are case-insensitive.


This module has no configuration directives.


Caveats/Known Bugs:
===================

This module can't be used together with imklog module. When using one of
them, make sure the other one is not enabled.

This is Linux specific module and requires /dev/kmsg device with
structured kernel logs.


Examples
========

The following sample pulls messages from the /dev/kmsg log device. All
parameters are left by default, which is usually a good idea. Please
note that loading the plugin is sufficient to activate it. No directive
is needed to start pulling messages.

.. code-block:: none

   $ModLoad imkmsg


