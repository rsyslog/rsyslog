.. _ref-templates:

Templates
=========

.. index::
   single: templates; overview
   single: schema mapping; templates
   single: data pipeline; templates

.. meta::
   :keywords: rsyslog, templates, list, subtree, string, plugin, schema mapping, data pipeline, jsonf, outname, ECS, mmjsonparse, mmleefparse, omelasticsearch, omfile

.. summary-start

Templates define how rsyslog transforms data before output.
They map parsed fields into schemas, format records, and generate dynamic destinations.

.. summary-end


Overview
--------

Templates are a central concept in rsyslog. They sit at the point where **parsed
messages are transformed into their final structure**. This includes:

- **Schema mapping in pipelines** — normalize or rename parsed fields
  into a structured schema (e.g., ECS JSON).
- **Output generation** — create custom message formats or dynamic filenames.
- **Textual transport formats** — add headers when targeting syslog or other
  line-based transports.

Every output in rsyslog, from files to Elasticsearch to remote syslog,
relies on templates. If no explicit template is bound, rsyslog uses built-in
defaults compatible with classic syslog.


Message pipeline integration
----------------------------

Templates appear between **parsing** and **actions**. They define what data
is sent forward:

.. mermaid::

   flowchart TD
      A["Input<br>(imudp, imtcp, imkafka)"]
      B["Parser (mmjsonparse,<br>mmleefparse)"]
      C["Template<br>list (mapping)"]
      D["Action<br>(omfile, omelasticsearch)"]
      A --> B --> C --> D

   %% Alternative when a prepared tree exists:
      E["Prepared tree<br>($!ecs)"] --> F["Template<br>subtree"] --> D
      A --> E
      B --> E


Choosing a template type
------------------------

- Use :ref:`ref-templates-type-list` to map fields one by one
  (rename keys, add or drop fields, inject constants) and build JSON safely
  with ``jsonf``.
- Use :ref:`ref-templates-type-subtree` to serialize a prepared
  JSON tree (for example, after :ref:`ref-mmjsonparse` or
  :ref:`ref-mmleefparse` populated ``$!ecs``).
- Use :ref:`ref-templates-type-string` for simple text records
  (legacy syslog lines, CSV, or other plain-text formats).
- Use :ref:`ref-templates-type-plugin` for special encodings
  provided by modules.


Schema mapping with JSONF
-------------------------

Modern pipelines should prefer structured JSON. The recommended method is:

- Enable JSON-safe encoding with ``option.jsonf="on``.
- For each field, use
  :ref:`property() <ref-templates-statement-property>` or
  :ref:`constant() <ref-templates-statement-constant>` with ``format="jsonf"``.
- For pre-normalized data, serialize a complete subtree such as ``$!ecs``
  with a :ref:`subtree template <ref-templates-type-subtree>`.

This ensures correct escaping and avoids handcrafted JSON concatenation.

Detailed examples are provided in the template type pages.


Templates for textual transports
--------------------------------

When the target transport expects **textual records** (for example,
classic syslog receivers, line-based file formats, or CSV),
the template must include a transport header. This is required so
receivers can parse message boundaries and metadata.

Typical cases:

- **Syslog protocols (RFC5424, RFC3164, protocol-23 draft)**
  Require a syslog header (timestamp, hostname, tag).
- **Text file formats**
  Such as ``RSYSLOG_FileFormat`` or ``RSYSLOG_TraditionalFileFormat``,
  which prepend headers before the message body.
- **Custom plain-text records**
  Where a template explicitly builds a line with fields separated by
  spaces, commas, or tabs.

Predefined reserved templates like ``RSYSLOG_ForwardFormat`` or
``RSYSLOG_SyslogProtocol23Format`` exist for these purposes.

If your output is a **structured JSON pipeline** (e.g. to Elasticsearch
or a file), you do not need to add any textual header.


.. _templates.template-object:

The ``template()`` object
-------------------------

Templates are defined with the ``template()`` object, a static construct
processed when rsyslog reads the configuration. Basic syntax:

.. code-block:: rsyslog

   template(name="..." type="...")

List templates additionally support a block form:

.. code-block:: rsyslog

   template(name="..." type="list" option.jsonf="on") {
     property(outname="field" name="msg" format="jsonf")
     constant(outname="@version" value="1" format="jsonf")
   }

See the type-specific subpages for details.


.. _templates.types:

Template types
--------------

.. list-table::
   :header-rows: 1
   :widths: 20 80

   * - Topic
     - Summary
   * - :ref:`ref-templates-type-list`
     - .. include:: ../reference/templates/templates-type-list.rst
        :start-after: .. summary-start
        :end-before: .. summary-end

   * - :ref:`ref-templates-type-subtree`
     - .. include:: ../reference/templates/templates-type-subtree.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`ref-templates-type-string`
     - .. include:: ../reference/templates/templates-type-string.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`ref-templates-type-plugin`
     - .. include:: ../reference/templates/templates-type-plugin.rst
        :start-after: .. summary-start
        :end-before: .. summary-end


.. _templates.statements:

Template statements
-------------------

.. list-table::
   :header-rows: 1
   :widths: 20 80

   * - Topic
     - Summary
   * - :ref:`ref-templates-statement-constant`
     - .. include:: ../reference/templates/templates-statement-constant.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`ref-templates-statement-property`
     - .. include:: ../reference/templates/templates-statement-property.rst
        :start-after: .. summary-start
        :end-before: .. summary-end


.. _templates.additional:

Additional topics
-----------------

.. list-table::
   :header-rows: 1
   :widths: 20 80

   * - Topic
     - Summary
   * - :ref:`ref-templates-options`
     - .. include:: ../reference/templates/templates-options.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`ref-templates-examples`
     - .. include:: ../reference/templates/templates-examples.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`ref-templates-reserved-names`
     - .. include:: ../reference/templates/templates-reserved-names.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`ref-templates-legacy`
     - .. include:: ../reference/templates/templates-legacy.rst
        :start-after: .. summary-start
        :end-before: .. summary-end


.. _templates.reserved-table:

Reserved template names overview
--------------------------------

These names provide predefined formats mainly for **compatibility**.
For modern JSON pipelines, prefer custom list or subtree templates.

.. list-table::
   :header-rows: 1
   :widths: 40 60

   * - Name
     - Purpose
   * - RSYSLOG_TraditionalFileFormat
     - Old style log file format
   * - RSYSLOG_FileFormat
     - Modern logfile format with high-precision timestamps
   * - RSYSLOG_TraditionalForwardFormat
     - Traditional forwarding with low-precision timestamps
   * - RSYSLOG_SysklogdFileFormat
     - Sysklogd compatible format
   * - RSYSLOG_ForwardFormat
     - High-precision forwarding format
   * - RSYSLOG_SyslogProtocol23Format
     - Format from IETF draft syslog-protocol-23
   * - RSYSLOG_DebugFormat
     - Troubleshooting format listing all properties
   * - RSYSLOG_WallFmt
     - Host and time followed by tag and message
   * - RSYSLOG_StdUsrMsgFmt
     - Syslogtag followed by the message
   * - RSYSLOG_StdDBFmt
     - Insert command for MariaDB/MySQL
   * - RSYSLOG_StdPgSQLFmt
     - Insert command for PostgreSQL
   * - RSYSLOG_spoofadr
     - Sender IP address only
   * - RSYSLOG_StdJSONFmt
     - JSON structure of message properties


Legacy ``$template`` statement
------------------------------

For historical configurations, the legacy ``$template`` syntax is still
recognized. See :ref:`ref-templates-legacy` for details.


See also
--------

- `How to bind a template <https://www.rsyslog.com/how-to-bind-a-template/>`_
- `Adding the BOM to a message <https://www.rsyslog.com/adding-the-bom-to-a-message/>`_
- `How to separate log files by host name of the sending device <https://www.rsyslog.com/article60/>`_


.. toctree::
   :hidden:

   ../reference/templates/templates-type-list
   ../reference/templates/templates-type-subtree
   ../reference/templates/templates-type-string
   ../reference/templates/templates-type-plugin
   ../reference/templates/templates-statement-constant
   ../reference/templates/templates-statement-property
   ../reference/templates/templates-options
   ../reference/templates/templates-examples
   ../reference/templates/templates-reserved-names
   ../reference/templates/templates-legacy
