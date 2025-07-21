**********************************
immark: Mark Message Input Module
**********************************

===========================  ===========================================================================
**Module Name:**             **immark**
**Author:**                  `Rainer Gerhards <https://rainer.gerhards.net/>`_ <rgerhards@adiscon.com>
===========================  ===========================================================================

Purpose
=======

This module provides the ability to inject periodic "mark" messages to
the input of rsyslog. This is useful to allow for verification that
the logging system is functioning.

Configuration Parameters
========================

.. note::

   Parameter names are case-insensitive.

Module Parameters
-----------------

interval
^^^^^^^^

.. csv-table::
   :header: "type", "default", "max", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "integer", "1200", "", "no", "``$MarkMessagePeriod``"

Specifies the mark message injection interval in seconds.

.. seealso::

   The Action Parameter ``action.writeAllMarkMessages`` in :doc:`../actions`.
