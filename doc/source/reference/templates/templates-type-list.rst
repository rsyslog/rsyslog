.. _ref-templates-type-list:
.. _templates.parameter.type-list:

List template type
==================

.. index::
   single: template; type=list
   single: templates; list type

.. meta::
   :keywords: rsyslog, template type, list, constant statement, property statement, JSON, schema mapping, data pipeline, ECS, LEEF

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
**property** statements enclosed in curly braces. Use it when you need to build
structured output **field by field** or perform explicit schema mapping.

- **Property statement** — emit values from rsyslog properties or variables into
  the output (see :ref:`ref-templates-statement-property`).
- **Constant statement** — set fixed values or inject literal text into the output
  (see :ref:`ref-templates-statement-constant`).

List templates work well for:

- **Schema mapping**: assign each output field one by one.
- **Structure-aware outputs** such as :ref:`ref-ommongodb` or :ref:`ref-omelasticsearch`.
- **Text outputs** such as :ref:`ref-omfile` where you need constant text (e.g., line breaks).

Compared to :ref:`ref-templates-type-subtree`, list templates are more verbose
but provide maximum control. Prefer list templates when you **don’t yet have**
a complete schema tree (e.g., while building an ECS mapping from scratch).

Generic data pipeline
--------------------------------------------------------------------------------

List templates are a key **data pipeline** step for mapping:

.. mermaid::

   flowchart TD
      A["Input<br>(imudp, imtcp, imkafka)"]
      B["Parser<br>(mmjsonparse, mmaudit)"]
      C["Template<br>list (mapping)"]
      D["Action<br>(omfile, omelasticsearch)"]
      A --> B --> C --> D

Example: simple ECS mapping (jsonftree)
--------------------------------------------------------------------------------

A minimal list template that emits selected ECS fields in JSON format.  Use
``option.jsonftree="on"`` so dotted ``outname`` values become nested objects
instead of flat strings:

.. code-block:: none

   template(name="ecs_min" type="list" option.jsonftree="on") {
     property(outname="@timestamp"     name="timereported"
              format="jsonf" dateFormat="rfc3339")
     property(outname="event.original" name="msg" format="jsonf")
     property(outname="host.hostname"  name="hostname" format="jsonf")
     property(outname="log.level"      name="syslogseverity-text" format="jsonf")
   }

This produces valid JSON without hand-crafted quoting or braces.

Example: fixing a field with a constant (jsonftree)
--------------------------------------------------------------------------------

Sometimes you need to set a **fixed JSON field** (e.g., a version marker or a tag).
Use a **constant** statement with `outname` and `format="jsonf"` so the encoder
handles quoting consistently:

.. code-block:: none

   template(name="ecs_fix" type="list" option.jsonftree="on") {
     property(outname="@timestamp"     name="timereported"
              format="jsonf" dateFormat="rfc3339")
     property(outname="event.original" name="msg" format="jsonf")
     /* fixed field via constant, encoded as JSON */
     constant(outname="@version" value="1" format="jsonf")
   }

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

The list template performs field-by-field mapping using ``jsonftree`` to keep
dotted field names properly nested:

.. code-block:: none

   template(name="outfmt" type="list" option.jsonftree="on") {
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
     property(outname="client.packets"          name="$!leef!fields!srcPackets"    format="jsonf" dataType="number")
     property(outname="source.packets"          name="$!leef!fields!srcPackets"    format="jsonf" dataType="number")
     property(outname="server.packets"          name="$!leef!fields!dstPackets"    format="jsonf" dataType="number")
     property(outname="destination.packets"     name="$!leef!fields!dstPackets"    format="jsonf" dataType="number")
     property(outname="observer.hostname"       name="$!leef!fields!DeviceName"    format="jsonf")
   }

Notes
--------------------------------------------------------------------------------

- Prefer `property(... format="jsonf")` for dynamic fields; use **`constant(outname=…, format="jsonf")`** for small fixed values.
- Best used when mapping output **field by field**.
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
