Log Message Normalization Module (mmnormalize)
==============================================

**Module Name:    mmnormalize**

**Available since:** 6.1.2+

**Author:** Rainer Gerhards <rgerhards@adiscon.com>

**Description**:

This module provides the capability to normalize log messages via
`liblognorm <http://www.liblognorm.com>`_. Thanks to liblognorm,
unstructured text, like usually found in log messages, can very quickly
be parsed and put into a normal form. This is done so quickly, that it
should be possible to normalize events in realtime.

This module is implemented via the output module interface. This means
that mmnormalize should be called just like an action. After it has been
called, the normalized message properties are available and can be
accessed. These properties are called the "CEE/lumberjack" properties,
because liblognorm creates a format that is inspired by the
CEE/lumberjack approach.

**Please note:** CEE/lumberjack properties are different from regular
properties. They have always "$!" prepended to the property name given
in the rulebase. Such a property needs to be called with
**%$!propertyname%**.

Note that mmnormalize should only be called once on each message.
Behaviour is undefined if multiple calls to mmnormalize happen for the
same message.

**Action Parameters**:

-  **ruleBase** [word]
   Specifies which rulebase file is to use. If there are multiple
   mmnormalize instances, each one can use a different file. However, a
   single instance can use only a single file. This parameter MUST be
   given, because normalization can only happen based on a rulebase. It
   is recommended that an absolute path name is given. Information on
   how to create the rulebase can be found in the `liblognorm
   manual <http://www.liblognorm.com/files/manual/index.html>`_.
-  **useRawMsg** [boolean]
   Specifies if the raw message should be used for normalization (on)
   or just the MSG part of the message (off). Default is "off".
-  **path** [word], defaults to "$!"
   Specifies the JSON path under which parsed elements should be
   placed. By default, all parsed properties are merged into root of
   message properties. You can place them under a subtree, instead. You
   can place them in local variables, also, by setting path="$.".
-  **variable** [word] *(Available since: 8.5.1)*
   Specifies if a variable insteed of property 'msg' should be used for
   normalization. A varible can be property, local variable, json-path etc.
   Please note that **useRawMsg** overrides this parameter, so if **useRawMsg**
   is set, **variable** will be ignored and raw message will be used.

   


**Legacy Configuration Directives**:

-  $mmnormalizeRuleBase <rulebase-file> - equivalent to the "ruleBase"
   parameter.
-  $mmnormalizeUseRawMsg <on/off> - equivalent to the "useRawMsg"
   parameter.

**See Also**

-  `First steps for
   mmnormalize <http://www.rsyslog.com/normalizer-first-steps-for-mmnormalize/>`_
-  `Log normalization and special
   characters <http://www.rsyslog.com/log-normalization-and-special-characters/>`_
-  `Log normalization and the leading
   space <http://www.rsyslog.com/log-normalization-and-the-leading-space/>`_
-  `Using mmnormalize effectively with Adiscon
   LogAnalyzer <http://www.rsyslog.com/using-rsyslog-mmnormalize-module-effectively-with-adiscon-loganalyzer/>`_

**Caveats/Known Bugs:**

None known at this time.

**Sample:**

This activates the module and applies normalization to all messages:

::

  module(load="mmnormalize")
  action(type="mmnormalize" ruleBase="/path/to/rulebase.rb")

The same in legacy format:

::

  $ModLoad mmnormalize
  $mmnormalizeRuleBase /path/to/rulebase.rb
  *.* :mmnormalize:
