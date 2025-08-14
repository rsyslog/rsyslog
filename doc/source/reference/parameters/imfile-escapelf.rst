.. _param-imfile-escapelf:
.. _imfile.parameter.input.escapelf:
.. _imfile.parameter.escapelf:

escapeLF
========

.. index::
   single: imfile; escapeLF
   single: escapeLF

.. summary-start

Escapes embedded LF characters in multi-line messages as ``#012`` when enabled.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imfile`.

:Name: escapeLF
:Scope: input
:Type: boolean
:Default: on
:Required?: no
:Introduced: at least 8.x, possibly earlier

Description
-----------
Only meaningful when multi-line messages are processed. LF characters inside
messages cause issues with many tools. If set to ``on``, LF characters are
escaped to the sequence ``#012``. By default escaping is enabled. If turned
off, test carefully. When using plain TCP syslog with embedded LFs, enable
octet-counted framing.

Input usage
-----------
.. _param-imfile-input-escapelf:
.. _imfile.parameter.input.escapelf-usage:

.. code-block:: rsyslog

   input(type="imfile" escapeLF="on")

Notes
-----
- Legacy documentation used the term ``binary`` for the type. It is treated as boolean.

See also
--------
See also :doc:`../../configuration/modules/imfile`.
