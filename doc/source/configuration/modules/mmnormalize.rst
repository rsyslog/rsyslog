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

Note that from a performance point of view mmnormalize should only be called
once on each message, if possible. To do so, place all rules into a single
rule base. If that is not possible, you can safely call mmnormalize multiple
times. This incurs a small performance drawback.

Module Parameters
~~~~~~~~~~~~~~~~~

.. note::

   Parameter names are case-insensitive; camelCase is recommended for readability.

.. list-table::
   :widths: 30 70
   :header-rows: 1

   * - Parameter
     - Summary
   * - :ref:`param-mmnormalize-allow-regex`
     - .. include:: ../../reference/parameters/mmnormalize-allow-regex.rst
        :start-after: .. summary-start
        :end-before: .. summary-end

Action Parameters
~~~~~~~~~~~~~~~~~

.. list-table::
   :widths: 30 70
   :header-rows: 1

   * - Parameter
     - Summary
   * - :ref:`param-mmnormalize-rulebase`
     - .. include:: ../../reference/parameters/mmnormalize-rulebase.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-mmnormalize-rule`
     - .. include:: ../../reference/parameters/mmnormalize-rule.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-mmnormalize-userawmsg`
     - .. include:: ../../reference/parameters/mmnormalize-userawmsg.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-mmnormalize-path`
     - .. include:: ../../reference/parameters/mmnormalize-path.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-mmnormalize-variable`
     - .. include:: ../../reference/parameters/mmnormalize-variable.rst
        :start-after: .. summary-start
        :end-before: .. summary-end

.. toctree::
   :hidden:

   ../../reference/parameters/mmnormalize-allow-regex
   ../../reference/parameters/mmnormalize-rulebase
   ../../reference/parameters/mmnormalize-rule
   ../../reference/parameters/mmnormalize-userawmsg
   ../../reference/parameters/mmnormalize-path
   ../../reference/parameters/mmnormalize-variable

See Also
~~~~~~~~

-  `First steps for
   mmnormalize <http://www.rsyslog.com/normalizer-first-steps-for-mmnormalize/>`_
-  `Log normalization and special
   characters <http://www.rsyslog.com/log-normalization-and-special-characters/>`_
-  `Log normalization and the leading
   space <http://www.rsyslog.com/log-normalization-and-the-leading-space/>`_
-  `Using mmnormalize effectively with Adiscon
   LogAnalyzer <http://www.rsyslog.com/using-rsyslog-mmnormalize-module-effectively-with-adiscon-loganalyzer/>`_

Caveats/Known Bugs
~~~~~~~~~~~~~~~~~~

None known at this time.

Example
~~~~~~~

**Sample 1:**

In this sample messages are received via imtcp. Then they are normalized with the given rulebase.
After that they are written in a file.

::

  module(load="mmnormalize")
  module(load="imtcp")

  input(type="imtcp" port="10514" ruleset="outp")

  ruleset(name="outp") {
  	action(type="mmnormalize" rulebase="/tmp/rules.rulebase")
  	action(type="omfile" File="/tmp/output")
  }

**Sample 2:**

In this sample messages are received via imtcp. Then they are normalized based on the given rules.
The strings from **rule** are put together and are equal to a rulebase with the same content.

::

  module(load="mmnormalize")
  module(load="imtcp")

  input(type="imtcp" port="10514" ruleset="outp")

  ruleset(name="outp") {
  	action(type="mmnormalize" rule=["rule=:%host:word% %tag:char-to:\\x3a%: no longer listening on %ip:ipv4%#%port:number%", "rule=:%host:word% %ip:ipv4% user was logged out"])
  	action(type="omfile" File="/tmp/output")
  }

**Sample 3:**

This activates the module and applies normalization to all messages:

::

  module(load="mmnormalize")
  action(type="mmnormalize" ruleBase="/path/to/rulebase.rb")

The same in legacy format:

::

  $ModLoad mmnormalize
  $mmnormalizeRuleBase /path/to/rulebase.rb
  *.* :mmnormalize:
