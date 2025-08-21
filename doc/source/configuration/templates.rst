Templates
=========

.. _templates.description:

Description
-----------

Templates are a key feature of rsyslog. They define arbitrary output
formats and enable dynamic file name generation. Every output, including
files, user messages, and database writes, relies on templates. When no
explicit template is set, rsyslog uses built-in defaults compatible with
stock syslogd formats. Key elements of templates are rsyslog
properties; see :doc:`rsyslog properties <properties>`.

.. _templates.template-processing:

Template processing
-------------------

When defining a template it should include a `HEADER` as defined in
`RFC5424 <https://datatracker.ietf.org/doc/html/rfc5424>`_. Understanding
:doc:`rsyslog parsing <parser>` is important. For example, if the ``MSG``
field is ``"this:is a message"`` and neither ``HOSTNAME`` nor ``TAG`` are
specified, the outgoing parser splits the message as:

.. code-block:: none

   TAG:this:
   MSG:is a message

.. _templates.template-object:

The ``template()`` object
-------------------------

Templates are defined with the ``template()`` object, which is a static
construct processed when rsyslog reads the configuration. Basic syntax:

.. code-block:: none

   template(parameters)

List templates additionally support an extended syntax:

.. code-block:: none

   template(parameters) { list-descriptions }

Parameters ``name`` and ``type`` select the template name and type. The
name must be unique. See below for available types and statements.

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
