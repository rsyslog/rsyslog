.. _pmnormalize:

.. meta::
   :description: Normalize incoming log messages with liblognorm and configure optional parser debug tracing.
   :keywords: rsyslog, pmnormalize, liblognorm, parser, normalization, debug

*********************************************
pmnormalize: Log Message Normalization parser
*********************************************

===========================  ===========================================================================
**Module Name:**             **pmnormalize**
**Author:**                  Pascal Withopf <pascalwithopf1@gmail.com>
**Available since:**         8.27.0
===========================  ===========================================================================

.. summary-start

Normalizes incoming messages with liblognorm and can emit per-parser
liblognorm debug records to rsyslog diagnostics or a dedicated file.

.. summary-end

Purpose
=======

This parser normalizes messages with the specified rules and populates the
properties for further use.


Configuration Parameters
========================

.. note::

   Parameter names are case-insensitive; camelCase is recommended for
   readability.

Parser Parameters
-----------------

rulebase
^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "word", "none", "no", "none"

Specifies which rulebase file is to use. If there are multiple
pmnormalize instances, each one can use a different file. However, a
single instance can use only a single file. This parameter or **rule**
MUST be given, because normalization can only happen based on a rulebase.
It is recommended that an absolute path name is given. Information on
how to create the rulebase can be found in the `liblognorm
manual <http://www.liblognorm.com/files/manual/index.html>`_.


rule
^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "array", "none", "no", "none"

Contains an array of strings which will be put together as the rulebase.
This parameter or **rulebase** MUST be given, because normalization can
only happen based on a rulebase.


undefinedPropertyError
^^^^^^^^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "binary", "off", "no", "none"

With this parameter an error message is controlled, which will be put out
every time pmnormalize can't normalize a message.


debug
^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "binary", "off", "no", "none"

Enables verbose liblognorm debugging for this parser. By default, trace records
are sent to rsyslog's internal diagnostic stream.

RainerScript usage:

.. code-block:: rsyslog

   parser(name="custom.pmnormalize" type="pmnormalize"
          rulebase="/path/to/rules.rb" debug="on")

YAML usage:

.. code-block:: yaml

   parsers:
     - name: custom.pmnormalize
       type: pmnormalize
       rulebase: /path/to/rules.rb
       debug: on


debugFile
^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "word", "none", "no", "none"

Redirects this parser's liblognorm debug records to the specified append-only
file. Requires **debug="on"** and causes configuration to fail if the file
cannot be opened.

RainerScript usage:

.. code-block:: rsyslog

   parser(name="custom.pmnormalize" type="pmnormalize"
          rulebase="/path/to/rules.rb" debug="on"
          debugFile="/path/to/pmnormalize-debug.log")

YAML usage:

.. code-block:: yaml

   parsers:
     - name: custom.pmnormalize
       type: pmnormalize
       rulebase: /path/to/rules.rb
       debug: on
       debugFile: /path/to/pmnormalize-debug.log


Examples
========

Normalize messages with rulebase
--------------------------------

In this sample messages are received via imtcp. Then they are normalized with
the given rulebase and written to a file.

.. code-block:: none

   module(load="imtcp")
   module(load="pmnormalize")

   input(type="imtcp" port="13514" ruleset="ruleset")

   parser(name="custom.pmnormalize" type="pmnormalize" rulebase="/tmp/rules.rulebase")

   ruleset(name="ruleset" parser="custom.pmnormalize") {
   	action(type="omfile" file="/tmp/output")
   }


Normalize messages with rules specified
---------------------------------------

Same as above, but messages are  normalized with the given rule array.

.. code-block:: none

   module(load="imtcp")
   module(load="pmnormalize")

   input(type="imtcp" port="10514" ruleset="outp")

   parser(name="custom.pmnormalize" type="pmnormalize" rule=[
   		"rule=:<%pri:number%> %fromhost-ip:ipv4% %hostname:word% %syslogtag:char-to:\\x3a%: %msg:rest%",
   		"rule=:<%pri:number%> %hostname:word% %fromhost-ip:ipv4% %syslogtag:char-to:\\x3a%: %msg:rest%"])

   ruleset(name="outp" parser="custom.pmnormalize") {
   	action(type="omfile" File="/tmp/output")
   }
