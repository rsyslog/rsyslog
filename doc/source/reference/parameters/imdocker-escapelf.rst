.. _param-imdocker-escapelf:
.. _imdocker.parameter.module.escapelf:

.. index::
   single: imdocker; escapeLF
   single: escapeLF

.. summary-start

Escapes embedded LF characters in messages as ``#012``.
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
When enabled, LF characters inside messages are replaced with the four-byte sequence ``#012`` to avoid issues with multi-line content.

.. _param-imdocker-module-escapelf:
.. _imdocker.parameter.module.escapelf-usage:

Module usage
------------
.. code-block:: rsyslog

   module(load="imdocker" escapeLF="...")

Notes
-----
This parameter was historically described as ``binary`` but acts as a boolean option.

See also
--------
See also :doc:`../../configuration/modules/imdocker`.
