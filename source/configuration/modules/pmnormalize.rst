Log Message Normalization Parser Module (pmnormalize)
=====================================================

**Module Name:    pmnormalize**

**Available since:** 8.27.0

**Author:** Pascal Withopf <pascalwithopf1@gmail.com>

**Description**:

This parser normalizes messages with the specified rules and populates the
properties for further use.

Action Parameters
~~~~~~~~~~~~~~~~~

Note: parameter names are case-insensitive.

.. function:: rulebase <word>

   Specifies which rulebase file is to use. If there are multiple
   pmnormalize instances, each one can use a different file. However, a
   single instance can use only a single file. This parameter or **rule**
   MUST be given, because normalization can only happen based on a rulebase.
   It is recommended that an absolute path name is given. Information on
   how to create the rulebase can be found in the `liblognorm
   manual <http://www.liblognorm.com/files/manual/index.html>`_.

.. function:: rule <array>

   Contains an array of strings which will be put together as the rulebase.
   This parameter or **rulebase** MUST be given, because normalization can
   only happen based on a rulebase.

.. function:: undefinedpropertyerror <on/off>

   **Default: off**

   With this parameter an error message is controlled, which will be put out
   every time pmnormalize can't normalize a message.

See Also
~~~~~~~~


Caveats/Known Bugs
~~~~~~~~~~~~~~~~~~

None known at this time.

Example
~~~~~~~

**Sample 1:**

In this sample messages are received via imtcp. Then they are normalized with
the given rulebase and written to a file.

::

  module(load="imtcp")
  module(load="pmnormalize")

  input(type="imtcp" port="13514" ruleset="ruleset")

  parser(name="custom.pmnormalize" type="pmnormalize" rulebase="/tmp/rules.rulebase")

  ruleset(name="ruleset" parser="custom.pmnormalize") {
  	action(type="omfile" file="/tmp/output")
  }

**Sample 2:**

In this sample messages are received via imtcp. Then they are normalized with
the given rule array. After that they are written in a file.

::

  module(load="imtcp")
  module(load="pmnormalize")

  input(type="imtcp" port="10514" ruleset="outp")

  parser(name="custom.pmnormalize" type="pmnormalize" rule=[
  		"rule=:<%pri:number%> %fromhost-ip:ipv4% %hostname:word% %syslogtag:char-to:\\x3a%: %msg:rest%",
  		"rule=:<%pri:number%> %hostname:word% %fromhost-ip:ipv4% %syslogtag:char-to:\\x3a%: %msg:rest%"])

  ruleset(name="outp" parser="custom.pmnormalize") {
  	action(type="omfile" File="/tmp/output")
  }
