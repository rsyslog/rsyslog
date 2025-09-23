.. _ref-templates-type-list:
.. _templates.parameter.type-list:

List template type
==================

.. index::
   single: template; type=list
   single: templates; list type

.. meta::
   :keywords: rsyslog, template type, list, constant statement, property statement, JSON, schema mapping, data pipeline, ECS, LEEF, Palo Alto

.. summary-start

List templates build output from a sequence of constant and property statements.
They are ideal for schema mapping when fields must be added one by one.

.. summary-end

:Name: ``type="list"``
:Scope: template
:Type: list
:Introduced: 5.9.6

Description
--------------------------------------------------------------------------------

The *list template type* generates output from a sequence of **constant** and
**property** statements enclosed in curly braces. This format is recommended
when complex substitutions are required, or when building structured documents
**field by field**.

List templates work well for:

- **Schema mapping**: explicitly assigning each output field, one by one.
- **Structure-aware outputs** such as :ref:`ref-ommongodb` or :ref:`ref-omelasticsearch`.
- **Text-based outputs** such as :ref:`ref-omfile` where constant text like
  separators and line breaks must be inserted.

Compared to :ref:`ref-templates-type-subtree`, list templates are more verbose
but provide maximum control. They are preferred when no complete schema tree
exists yet (e.g., when building an ECS mapping from scratch).

Generic data pipeline
--------------------------------------------------------------------------------

List templates are a key step in rsyslog **data pipelines**, where they are used
for schema mapping.

.. mermaid::

   flowchart TD
      A["Input<br>(imudp, imtcp, imkafka)"]
      B["Parser<br>(mmjsonparse, mmaudit)"]
      C["Template<br>list (mapping)"]
      D["Action<br>(omfile, omkafka, ...)"]
      A --> B --> C --> D

Example: simple ECS mapping
--------------------------------------------------------------------------------

A minimal list template that emits selected ECS fields in JSON format:

.. code-block:: none

   template(name="ecs_min" type="list" option.jsonf="on") {
     property(outname="@timestamp"     name="timereported"
              format="jsonf" dateFormat="rfc3339")
     property(outname="event.original" name="msg" format="jsonf")
     property(outname="host.hostname"  name="hostname" format="jsonf")
     property(outname="log.level"      name="syslogseverity-text" format="jsonf")
   }

This produces valid JSON with the given fields, letting rsyslog handle all
quoting and escaping.

Example: Palo Alto firewall (LEEF → ECS)
--------------------------------------------------------------------------------

A practical case is mapping Palo Alto firewall logs into ECS fields.
The typical workflow looks like this:

.. mermaid::

   flowchart TD
      A["Input<br>(imtcp)"]
      B["Parser<br>(mmleefparse)"]
      C["Template<br>list (LEEF→ECS mapping)"]
      D["Action<br>(omelasticsearch)"]
      A --> B --> C --> D

The list template itself uses JSON output (`option.jsonf="on"`) and performs
field-by-field mapping:

.. code-block:: none

   template(name="outfmt" type="list" option.jsonf="on") {
     property(outname="@timestamp"              name="timereported"
              format="jsonf" dateFormat="rfc3339")
     property(outname="event.created"           name="$!leef!fields!ReceiveTime"   format="jsonf")
     property(outname="observer.serial_number"  name="$!leef!fields!SerialNumber"  format="jsonf")
     property(outname="event.category"          name="$!leef!fields!Type"          format="jsonf")
     property(outname="event.action"            name="$!leef!fields!Subtype"       format="jsonf")
     property(outname="client.ip"               name="$!leef!fields!src"           format="jsonf")
     property(outname="source.ip"               name="$!leef!fields!src"           format="jsonf")
     property(outname="server.ip"               name="$!leef!fields!dst"           format="jsonf")
     property(outname="destination.ip"          name="$!leef!fields!dst"           format="jsonf")
     property(outname="client.user.name"        name="$!leef!fields!usrName"       format="jsonf")
     property(outname="source.user.name"        name="$!leef!fields!usrName"       format="jsonf")
     property(outname="server.user.name"        name="$!leef!fields!DestinationUser" format="jsonf")
     property(outname="destination.user.name"   name="$!leef!fields!DestinationUser" format="jsonf")
     property(outname="network.application"     name="$!leef!fields!Application"   format="jsonf")
     property(outname="client.port"             name="$!leef!fields!srcPort"       format="jsonf" dataType="number")
     property(outname="source.port"             name="$!leef!fields!srcPort"       format="jsonf" dataType="number")
     property(outname="destination.port"        name="$!leef!fields!dstPort"       format="jsonf" dataType="number")
     property(outname="server.port"             name="$!leef!fields!dstPort"       format="jsonf" dataType="number")
     property(outname="labels"                  name="$!leef!fields!Flags"         format="jsonf")
     property(outname="network.transport"       name="$!leef!fields!proto"         format="jsonf")
     property(outname="event.outcome"           name="$!leef!fields!action"        format="jsonf")
     property(outname="network.bytes"           name="$!leef!fields!totalBytes"    format="jsonf" dataType="number")
     property(outname="client.bytes"            name="$!leef!fields!srcBytes"      format="jsonf" dataType="number")
     property(outname="source.bytes"            name="$!leef!fields!srcBytes"      format="jsonf" dataType="number")
     property(outname="server.bytes"            name="$!leef!fields!dstBytes"      format="jsonf" dataType="number")
     property(outname="destination.bytes"       name="$!leef!fields!dstBytes"      format="jsonf" dataType="number")
     property(outname="network.packets"         name="$!leef!fields!totalPackets"  format="jsonf" dataType="number")
     property(outname="event.start"             name="$!leef!fields!StartTime"     format="jsonf")
     property(outname="event.duration"          name="$!leef!fields!ElapsedTime"   format="jsonf" dataType="number")
     property(outname="server.packets"          name="$!leef!fields!srcPackets"    format="jsonf" dataType="number")
     property(outname="destination.packets"     name="$!leef!fields!srcPackets"    format="jsonf" dataType="number")
     property(outname="client.packets"          name="$!leef!fields!dstPackets"    format="jsonf" dataType="number")
     property(outname="source.packets"          name="$!leef!fields!dstPackets"    format="jsonf" dataType="number")
     property(outname="observer.hostname"       name="$!leef!fields!DeviceName"    format="jsonf")
   }

This demonstrates a **real production mapping**, where each firewall field
is translated into its ECS equivalent.

Notes
--------------------------------------------------------------------------------

- List templates support **constant text**, unlike subtree templates.
- Best used when mapping output **field by field**.
- More verbose than subtree templates, but more flexible.
- For complete schema trees, prefer :ref:`ref-templates-type-subtree`.

See also
--------------------------------------------------------------------------------

- :ref:`ref-templates-type-subtree`
- :ref:`ref-templates-statement-constant`
- :ref:`ref-templates-statement-property`
- :ref:`ref-mmleefparse`
- :ref:`ref-ommongodb`
- :ref:`ref-omelasticsearch`
- :ref:`ref-omfile`
- :ref:`ref-templates`
