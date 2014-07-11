pmciscoios
==========

Author: Rainer Gerhards

Available since: 8.3.4+

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

Parser Parameters
-----------------

.. function::  present.origin <boolean>

   **Default**: off

   This setting tell the parser if the origin field is present inside
   the message. Due to the nature of Cisco's logging format, the parser
   cannot sufficiently correctly deduce if the origin field is present
   or not (at least not with reasonable performance). As such, the parser
   must be provided with that information. If the origin is present,
   its value is stored inside the HOSTNAME message property.

Example
-------
We assume a scenario where we have some devices configured to emit origin
information whereas some others do not. In order to differentiate between
the two classes, rsyslog accepts input on different ports, one per class.
For each port, an input() object is defined, which binds the port to a
ruleset that uses the appropriately-configured parser. Except for the
different parsers, processing shall be identical for both classes. In our
first example we do this via a common ruleset which carries out the
actual processing:

::

   module(load="imtcp")
   module(load="pmciscoios")

   input(type="imtcp" port="10514" ruleset="withoutOrigin")
   input(type="imtcp" port="10515" ruleset="withOrigin")

   ruleset(name="common") {
       ... do processing here ...
   }

   ruleset(name="withoutOrigin" parser="rsyslog.pmciscoios") {
       /* this ruleset uses the default parser which was
        * created during module load
        */
       call common
   }

   parser(name="custom.pmciscoios.withOrigin" type="pmciscoios"
          present.origin="on")
   ruleset(name="withOrigin" parser="custom.pmciscoios.withOrigin") {
       /* this ruleset uses the parser defined immediately above */
       call common
   }


The example configuration above is a good solution. However, it is possible
to do the same thing in a somewhat condensed way, but if and only if the date
stamp immediately follows the origin. In that case, the parser has a chance to
detect if the origin is present or not. The key point here is to make sure
the parser checking for the origin is given before the default one, in which
case the first on will detect it does not match an pass on to the next
one inside the parser chain. However, this comes at the expense of additional
runtime overhead. The example below is **not** good practice -- it is given
as a purely educational sample to show some fine details of how parser
definitions interact. In this case, we can use a single listener.

::

   module(load="imtcp")
   module(load="pmciscoios")

   input(type="imtcp" port="10514" ruleset="ciscoBoth")

   parser(name="custom.pmciscoios.withOrigin" type="pmciscoios"
          present.origin="on")
   ruleset(name="ciscoBoth"
           parser=["custom.pmciscoios.withOrigin", "rsyslog.pmciscoios"]) {
       ... do processing here ...
   }
