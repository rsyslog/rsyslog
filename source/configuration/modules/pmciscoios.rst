**********
pmciscoios
**********

===========================  ===========================================================================
**Module Name:**             **pmciscoios**
**Author:**                  `Rainer Gerhards <https://rainer.gerhards.net/>`_ <rgerhards@adiscon.com>
**Available since:**         8.3.4+
===========================  ===========================================================================


Purpose
=======

This is a parser that understands Cisco IOS "syslog" format. Note
that this format is quite different from RFC syslog format, and
so the default parser chain cannot deal with it.

Note that due to large differences in IOS logging format, pmciscoios
may currently not be able to handle all possible format variations.
Nevertheless, it should be fairly easy to adapt it to additional
requirements. So be sure to ask if you run into problems with
format issues.

Note that if your Cisco system emits timezone information in a supported
format, rsyslog will pick it up. In order to apply proper timezone offsets,
the timezone ids (e.g. "EST") must be configured via the
:doc:`timezone object <../timezone>`.

Note if the clock on the Cisco device has not been set and cannot be
verified the Cisco will prepend the timestamp field with an asterisk (*).
If the clock has gone out of sync with its configured NTP server the
timestamp field will be prepended with a dot (.). In both of these cases
parsing the timestamp would fail, therefore any preceding asterisks (*) or
dots (.) are ignored. This may lead to "incorrect" timestamps being logged.


Configuration Parameters
========================

.. note::

   Parameter names are case-insensitive.


Parser Parameters
-----------------

present.origin
^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "binary", "off", "no", "none"

This setting tell the parser if the origin field is present inside
the message. Due to the nature of Cisco's logging format, the parser
cannot sufficiently correctly deduce if the origin field is present
or not (at least not with reasonable performance). As such, the parser
must be provided with that information. If the origin is present,
its value is stored inside the HOSTNAME message property.


present.xr
^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "binary", "off", "no", "none"

If syslog is received from an IOSXR device the syslog format will usually
start with the RSP/LC/etc that produced the log, then the timestamp.
It will also contain an additional syslog tag before the standard Cisco
%TAG, this tag references the process that produced the log.
In order to use this Cisco IOS parser module with XR format messages both
of these additional fields must be ignored.


Examples
========

Listening multiple devices, some emitting origin information and some not
-------------------------------------------------------------------------

We assume a scenario where we have some devices configured to emit origin
information whereas some others do not. In order to differentiate between
the two classes, rsyslog accepts input on different ports, one per class.
For each port, an input() object is defined, which binds the port to a
ruleset that uses the appropriately-configured parser. Except for the
different parsers, processing shall be identical for both classes. In our
first example we do this via a common ruleset which carries out the
actual processing:

.. code-block:: none

   module(load="imtcp")
   module(load="pmciscoios")

   input(type="imtcp" port="10514" ruleset="withoutOrigin")
   input(type="imtcp" port="10515" ruleset="withOrigin")

   ruleset(name="common") {
       ... do processing here ...
   }

   ruleset(name="withoutOrigin" parser="rsyslog.ciscoios") {
       /* this ruleset uses the default parser which was
        * created during module load
        */
       call common
   }

   parser(name="custom.ciscoios.withOrigin" type="pmciscoios"
          present.origin="on")
   ruleset(name="withOrigin" parser="custom.ciscoios.withOrigin") {
       /* this ruleset uses the parser defined immediately above */
       call common
   }


Date stamp immediately following the origin
-------------------------------------------

The example configuration above is a good solution. However, it is possible
to do the same thing in a somewhat condensed way, but if and only if the date
stamp immediately follows the origin. In that case, the parser has a chance to
detect if the origin is present or not. The key point here is to make sure
the parser checking for the origin is given before the default one, in which
case the first one will detect that it does not match and pass it on to the
next one in the parser chain. However, this comes at the expense of additional
runtime overhead. The example below is **not** good practice -- it is given
as a purely educational sample to show some fine details of how parser
definitions interact. In this case, we can use a single listener.

.. code-block:: none

   module(load="imtcp")
   module(load="pmciscoios")

   input(type="imtcp" port="10514" ruleset="ciscoBoth")

   parser(name="custom.ciscoios.withOrigin" type="pmciscoios"
          present.origin="on")
   ruleset(name="ciscoBoth"
           parser=["custom.ciscoios.withOrigin", "rsyslog.ciscoios"]) {
       ... do processing here ...
   }


Handling Cisco IOS and IOSXR formats
------------------------------------

The following sample demonstrates how to handle Cisco IOS and IOSXR formats

.. code-block:: none

   module(load="imudp")
   module(load="pmciscoios")

   input(type="imudp" port="10514" ruleset="ios")
   input(type="imudp" port="10515" ruleset="iosxr")

   ruleset(name="common") {
       ... do processing here ...
   }

   ruleset(name="ios" parser="rsyslog.ciscoios") {
       call common
   }

   parser(name="custom.ciscoios.withXr" type="pmciscoios"
          present.xr="on")
   ruleset(name="iosxr" parser="custom.ciscoios.withXr"] {
       call common
   }


