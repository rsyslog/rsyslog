Parser
======

.. index:: ! parser
.. _cfgobj_input:

The ``parser`` object, as its name suggests, describes message parsers.
Message parsers have a standard parser name, which can be used by simply
loading the parser module. Only when specific parameters need to be set
the parser object is needed.

In that case, it is used to define a new parser name (aka "parser definition")
which configures this name to use the parser module with set parameters.
This is important as the ruleset() object does not support to set parser
parameters. Instead, if parameters are needed, a proper parser name must
be defined using the parser() object. A parser name defined via the
parser() object can be used wherever a parser name can occur.

Note that not all message parser modules are supported in the parser()
object. The reason is that many do not have any user-selectable
parameters and as such, there is no point in issuing a parser() object
for them.

The parser object has different parameters:

-  those that apply to all parser and are generally available for
   all of them. These are documented below.
-  parser-specific parameters. These are specific to a certain parser
   module. They are documented by the :doc:`parser module <modules/idx_parser>`
   in question.


General Parser Parameters
-------------------------

Note: parameter names are case-insensitive.

.. function::  name <name-string>

   *Mandatory*

   This names the parser. Names starting with "rsyslog." are reserved for
   rsyslog use and must not be used. It is suggested to replace "rsyslog."
   with "custom." and keep the rest of the name descriptive. However, this
   is not enforced and just good practice.

.. function::  type <type-string>

   *Mandatory*

   The ``<type-string>`` is a string identifying the parser module as given
   it each module's documentation. Do not mistake the parser module name
   with its default parser name.
   For example, the
   :doc:`Cisco IOS message parser module <modules/pmciscoios>` parser module
   name is "pmciscoios", whereas it's default parser name is
   "rsyslog.pmciscoios".

Samples
-------
The following example creates a custom parser definition and uses it within a ruleset:

::

  module(load="pmciscoios")
  parser(name="custom.pmciscoios.with_origin" type="pmciscoios")

  ruleset(name="myRuleset" parser="custom.pmciscoios.with_origin") {
     ... do something here ...
  }

The following example uses multiple parsers within a ruleset without a parser object (the order is important):

::

  module(load="pmaixforwardedfrom")
  module(load="pmlastmsg")

  ruleset(name="myRuleset" parser=["rsyslog.lastline","rsyslog.aixforwardedfrom","rsyslog.rfc5424","rsyslog.rfc3164"]) {
     ... do something here ...
  }



A more elaborate example can also be found in the
:doc:`Cisco IOS message parser module <modules/pmciscoios>` documentation.

