.. _param-imfile-trimlineoverbytes:
.. _imfile.parameter.module.trimlineoverbytes:

trimLineOverBytes
=================

.. index::
   single: imfile; trimLineOverBytes
   single: trimLineOverBytes

.. summary-start

This is used to tell rsyslog to truncate the line which length is greater than specified bytes.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imfile`.

:Name: trimLineOverBytes
:Scope: input
:Type: integer
:Default: input=0
:Required?: no
:Introduced: at least 8.x, possibly earlier

Description
-----------
This is used to tell rsyslog to truncate the line which length is greater
than specified bytes. If it is positive number, rsyslog truncate the line
at specified bytes. Default value of 'trimLineOverBytes' is 0, means never
truncate line.

This option can be used when ``readMode`` is 0 or 2.

Input usage
-----------
.. _param-imfile-input-trimlineoverbytes:
.. _imfile.parameter.input.trimlineoverbytes:
.. code-block:: rsyslog

   input(type="imfile" trimLineOverBytes="...")

See also
--------
See also :doc:`../../configuration/modules/imfile`.
