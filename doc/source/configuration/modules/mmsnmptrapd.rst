mmsnmptrapd message modification module
=======================================

**Module Name:** mmsnmptrapd

**Author:** Rainer Gerhards <rgerhards@adiscon.com> (custom-created)

**Multi-Ruleset Support:** since 5.8.1

**Description**:

This module uses a specific configuration of snmptrapd's tag values to
obtain information of the original source system and the severity
present inside the original SNMP trap. It then replaces these fields
inside the syslog message.

Let's look at an example. Essentially, SNMPTT will invoke something like
this:

::

    logger -t snmptrapd/warning/realhost Host 003c.abcd.ffff in vlan 17 is flapping between port Gi4/1 and port Gi3/2

This message modification module will change the tag (removing the
additional information), hostname and severity (not shown in example),
so the log entry will look as follows:

::

    2011-04-21T16:43:09.101633+02:00 realhost snmptrapd: Host 003c.abcd.ffff in vlan 122 is flapping between port Gi4/1 and port Gi3/2

The following logic is applied to all message being processed:

#. The module checks incoming syslog entries. If their TAG field starts
   with "snmptrapd/" (configurable), they are modified, otherwise not.
   If the are modified, this happens as follows:
#. It will derive the hostname from the tag field which has format
   snmptrapd/severity/hostname
#. It should derive the severity from the tag field which has format
   snmptrapd/severity/hostname. A configurable mapping table will be
   used to drive a new severity value from that severity string. If no
   mapping has been defined, the original severity is not changed.
#. It replaces the "FromHost" value with the derived value from step 2
#. It replaces the "Severity" value with the derived value from step 3

Note that the placement of this module inside the configuration is
important. All actions before this modules is called will work on the
unmodified message. All messages after it's call will work on the
modified message. Please also note that there is some extra power in
case it is required: as this module is implemented via the output module
interface, a filter can be used (actually must be used) in order to tell
when it is called. Usually, the catch-all filter (\*.\*) is used, but
more specific filters are fully supported. So it is possible to define
different parameters for this module depending on different filters. It
is also possible to just run messages from one remote system through
this module, with the help of filters or multiple rulesets and ruleset
bindings. In short words, all capabilities rsyslog offers to control
output modules are also available to mmsnmptrapd.

Module Parameters
-----------------

.. note::

   Parameter names are case-insensitive; camelCase is recommended for
   readability.

.. list-table::
   :widths: 30 70
   :header-rows: 1

   * - Parameter
     - Summary
   * - :ref:`param-mmsnmptrapd-tag`
     - .. include:: ../../reference/parameters/mmsnmptrapd-tag.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-mmsnmptrapd-severitymapping`
     - .. include:: ../../reference/parameters/mmsnmptrapd-severitymapping.rst
        :start-after: .. summary-start
        :end-before: .. summary-end

.. toctree::
   :hidden:

   ../../reference/parameters/mmsnmptrapd-tag
   ../../reference/parameters/mmsnmptrapd-severitymapping

**Caveats/Known Bugs:**

-  currently none known

**Example:**

This enables to rewrite messages from snmptrapd and configures error and
warning severities. The default tag is used.

::

  module(load="mmsnmptrapd" severityMapping="warning/4,error/3") # needs to be done just once
  # ... other module loads and listener setup ...
  *.* /path/to/file/with/originalMessage # this file receives unmodified messages
  *.* :mmsnmptrapd: # now message is modified
  *.* /path/to/file/with/modifiedMessage # this file receives modified messages
  # ... rest of config ...

