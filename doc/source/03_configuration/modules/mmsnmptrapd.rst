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

**Configuration Parameters**:

Note: parameter names are case-insensitive.

-  **$mmsnmptrapdTag** [tagname]

   Tells the module which start string inside the tag to look for. The
   default is "snmptrapd". Note that a slash is automatically added to
   this tag when it comes to matching incoming messages. It MUST not be
   given, except if two slashes are required for whatever reasons (so
   "tag/" results in a check for "tag//" at the start of the tag field).

-  **$mmsnmptrapdSeverityMapping** [severitymap]
   This specifies the severity mapping table. It needs to be specified
   as a list. Note that due to the current config system **no
   whitespace** is supported inside the list, so be sure not to use any
   whitespace inside it.
   The list is constructed of Severity-Name/Severity-Value pairs,
   delimited by comma. Severity-Name is a case-sensitive string, e.g.
   "warning" and an associated numerical value (e.g. 4). Possible values
   are in the rage 0..7 and are defined in RFC5424, table 2. The given
   sample would be specified as "warning/4".
   If multiple instances of mmsnmptrapd are used, each instance uses
   the most recently defined $mmsnmptrapdSeverityMapping before itself.

**Caveats/Known Bugs:**

-  currently none known

**Example:**

This enables to rewrite messages from snmptrapd and configures error and
warning severities. The default tag is used.

::

  $ModLoad mmsnmptrapd # needs to be done just once
  # ... other module loads and listener setup ...
  *.* /path/to/file/with/originalMessage # this file receives unmodified messages
  $mmsnmptrapdSeverityMapping warning/4,error/3
  *.* :mmsnmptrapd: # now message is modified
  *.* /path/to/file/with/modifiedMessage # this file receives modified messages
  # ... rest of config ...

