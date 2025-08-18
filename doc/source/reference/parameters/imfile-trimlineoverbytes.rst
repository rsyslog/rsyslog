.. _param-imfile-trimlineoverbytes:
.. _imfile.parameter.input.trimlineoverbytes:
.. _imfile.parameter.trimlineoverbytes:

trimLineOverBytes
=================

.. index::
   single: imfile; trimLineOverBytes
   single: trimLineOverBytes

.. summary-start

Truncates lines longer than the specified number of bytes.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imfile`.

:Name: trimLineOverBytes
:Scope: input
:Type: integer
:Default: 0
:Required?: no
:Introduced: at least 8.x, possibly earlier

Description
-----------
Tells rsyslog to truncate lines whose length exceeds the configured number
of bytes. A positive value trims the line at that byte count. The default
``0`` means lines are never truncated. This option can be used when
``readMode`` is ``0`` or ``2``.

Input usage
-----------
.. _param-imfile-input-trimlineoverbytes:
.. _imfile.parameter.input.trimlineoverbytes-usage:

.. code-block:: rsyslog

   input(type="imfile"
         File="/var/log/example.log"
         Tag="example"
         trimLineOverBytes="0")

See also
--------
See also :doc:`../../configuration/modules/imfile`.
