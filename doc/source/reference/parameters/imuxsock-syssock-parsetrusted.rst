.. _param-imuxsock-syssock-parsetrusted:
.. _imuxsock.parameter.module.syssock-parsetrusted:

SysSock.ParseTrusted
====================

.. index::
   single: imuxsock; SysSock.ParseTrusted
   single: SysSock.ParseTrusted

.. summary-start

Turns trusted properties into JSON fields when annotation is enabled.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imuxsock`.

:Name: SysSock.ParseTrusted
:Scope: module
:Type: boolean
:Default: module=off
:Required?: no
:Introduced: 6.5.0

Description
-----------
If ``SysSock.Annotation`` is turned on, create JSON/lumberjack properties
out of the trusted properties (which can be accessed via |FmtAdvancedName|
JSON Variables, e.g. ``$!pid``) instead of adding them to the message.

.. versionadded:: 7.2.7
   |FmtAdvancedName| directive introduced

.. versionadded:: 7.3.8
   |FmtAdvancedName| directive introduced

.. versionadded:: 6.5.0
   |FmtObsoleteName| directive introduced

Module usage
------------
.. _param-imuxsock-module-syssock-parsetrusted:
.. _imuxsock.parameter.module.syssock-parsetrusted-usage:

.. code-block:: rsyslog

   module(load="imuxsock" sysSock.parseTrusted="on")

Legacy names (for reference)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Historic names/directives for compatibility. Do not use in new configs.

.. _imuxsock.parameter.legacy.systemlogparsetrusted:

- $SystemLogParseTrusted — maps to SysSock.ParseTrusted (status: legacy)

.. index::
   single: imuxsock; $SystemLogParseTrusted
   single: $SystemLogParseTrusted

See also
--------
See also :doc:`../../configuration/modules/imuxsock`.
