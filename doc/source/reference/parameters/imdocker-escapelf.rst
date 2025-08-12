.. _param-imdocker-escapelf:
.. _imdocker.parameter.module.escapelf:

escapeLF
========

.. index::
   single: imdocker; escapeLF
   single: escapeLF

.. summary-start

Escapes line feeds as ``#012`` in multi-line messages; default ``on``.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imdocker`.

:Name: escapeLF
:Scope: module
:Type: boolean
:Default: module=on
:Required?: no
:Introduced: 8.41.0

Description
-----------
If enabled, line feed characters embedded in messages are escaped to the
sequence ``#012``. This avoids issues with tools that do not expect embedded LF
characters.

Module usage
------------
.. _param-imdocker-module-escapelf:
.. _imdocker.parameter.module.escapelf-usage:

.. code-block:: rsyslog

   module(load="imdocker" escapeLF="...")

Notes
-----
- The original documentation described this as a binary option; it is a boolean parameter.

See also
--------
See also :doc:`../../configuration/modules/imdocker`.

