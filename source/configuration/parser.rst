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
parser() object can be used whereever a parser name can occur.

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

Sample
------
The following example creates a custom parser definition and uses it within a ruleset:

::

  load(module="pmciscoios")
  parser(name="custom.pmciscoios.with_origin" type="pmciscoios")

  ruleset(name="myRuleset" parser="custom.pmciscoios.with_origin") {
     ... do something here ...
  }


This documentation is part of the `rsyslog <http://www.rsyslog.com/>`_
project.
Copyright © 2014 by `Rainer Gerhards <http://www.gerhards.net/rainer>`_ and
`Adiscon <http://www.adiscon.com/>`_. Released under the GNU GPL version
2 or higher.
