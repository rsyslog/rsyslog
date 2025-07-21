*****************************************************
Log Message Normalization Parser Module (pmnormalize)
*****************************************************

===========================  ===========================================================================
**Module Name:**             **pmnormalize**
**Author:**                  Pascal Withopf <pascalwithopf1@gmail.com>
**Available since:**         8.27.0
===========================  ===========================================================================


Purpose
=======

This parser normalizes messages with the specified rules and populates the
properties for further use.


Configuration Parameters
========================

.. note::

   Parameter names are case-insensitive.


Action Parameters
-----------------

Rulebase
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


Rule
^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "array", "none", "no", "none"

Contains an array of strings which will be put together as the rulebase.
This parameter or **rulebase** MUST be given, because normalization can
only happen based on a rulebase.


UndefinedPropertyError
^^^^^^^^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "binary", "off", "no", "none"

With this parameter an error message is controlled, which will be put out
every time pmnormalize can't normalize a message.


Examples
========

Normalize msgs received via imtcp
---------------------------------

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


Write normalized messages to file
---------------------------------

In this sample messages are received via imtcp. Then they are normalized with
the given rule array. After that they are written in a file.

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

