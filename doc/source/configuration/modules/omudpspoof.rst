**************************************
omudpspoof: UDP spoofing output module
**************************************

===========================  ===========================================================================
**Module Name:**             **omudpspoof**
**Author:**                  David Lang <david@lang.hm> and `Rainer Gerhards <https://rainer.gerhards.net/>`_ <rgerhards@adiscon.com>
**Available Since:**         5.1.3
===========================  ===========================================================================


Purpose
=======

This module is similar to the regular UDP forwarder, but permits to
spoof the sender address. Also, it enables to circle through a number of
source ports.

**Important**: This module **requires root permissions**. This is a hard
requirement because raw socket access is necessary to fake UDP sender
addresses. As such, rsyslog cannot drop privileges if this module is
to be used. Ensure that you do **not** use `$PrivDropToUser` or
`$PrivDropToGroup`. Many distro default configurations (notably Ubuntu)
contain these statements. You need to remove or comment them out if you
want to use `omudpspoof`.


Configuration Parameters
========================

.. note::

   Parameter names are case-insensitive.

Module Parameters
-----------------

Template
^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "word", "RSYSLOG_TraditionalForwardFormat", "no", "none"

This setting instructs omudpspoof to use a template different from
the default template for all of its actions that do not have a
template specified explicitly.


Action Parameters
-----------------

Target
^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "word", "none", "yes", "``$ActionOMUDPSpoofTargetHost``"

Host that the messages shall be sent to.


Port
^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "word", "514", "no", "``$ActionOMUDPSpoofTargetPort``"

Remote port that the messages shall be sent to. Default is 514.


SourceTemplate
^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "word", "RSYSLOG_omudpspoofDfltSourceTpl", "no", "``$ActionOMUDPSpoofSourceNameTemplate``"

This is the name of the template that contains a numerical IP
address that is to be used as the source system IP address. While it
may often be a constant value, it can be generated as usual via the
property replacer, as long as it is a valid IPv4 address. If not
specified, the built-in default template
RSYSLOG\_omudpspoofDfltSourceTpl is used. This template is defined as
follows:
$template RSYSLOG\_omudpspoofDfltSourceTpl,"%fromhost-ip%"
So in essence, the default template spoofs the address of the system
the message was received from. This is considered the most important
use case.


SourcePort.start
^^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "integer", "32000", "no", "``$ActionOMUDPSpoofSourcePortStart``"

Specify the start value for circling the source ports. Start must be
less than or equal to sourcePort.End.


SourcePort.End
^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "integer", "42000", "no", "``$ActionOMUDPSpoofSourcePortEnd``"

Specify the end value for circling the source ports. End must be
equal to or more than sourcePort.Start.


MTU
^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "integer", "1500", "no", "none"

Maximum packet length to send.


Template
^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "word", "RSYSLOG_TraditionalForwardFormat", "no", "``$ActionOMUDPSpoofDefaultTemplate``"

This setting instructs omudpspoof to use a template different from
the default template for all of its actions that do not have a
template specified explicitly.


Caveats/Known Bugs
==================

-  **IPv6** is currently not supported. If you need this capability,
   please let us know via the rsyslog mailing list.

-  Throughput is MUCH smaller than when using omfwd module.


Examples
========

Forwarding message through multiple ports
-----------------------------------------

Forward the message to 192.168.1.1, using original source and port between 10000 and 19999.

.. code-block:: none

   Action (
     type="omudpspoof"
     target="192.168.1.1"
     sourceport.start="10000"
     sourceport.end="19999"
   )


Forwarding message using another source address
-----------------------------------------------

Forward the message to 192.168.1.1, using source address 192.168.111.111 and default ports.

.. code-block:: none

   Module (
     load="omudpspoof"
   )
   Template (
     name="spoofaddr"
     type="string"
     string="192.168.111.111"
   )
   Action (
     type="omudpspoof"
     target="192.168.1.1"
     sourcetemplate="spoofaddr"
   )


