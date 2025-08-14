.. _param-imfile-maxsubmitatonce:
.. _imfile.parameter.module.maxsubmitatonce:

MaxSubmitAtOnce
===============

.. index::
   single: imfile; MaxSubmitAtOnce
   single: MaxSubmitAtOnce

.. summary-start

This is an expert option.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imfile`.

:Name: MaxSubmitAtOnce
:Scope: input
:Type: integer
:Default: input=1024
:Required?: no
:Introduced: at least 8.x, possibly earlier

Description
-----------
This is an expert option. It can be used to set the maximum input
batch size that imfile can generate. The **default** is 1024, which
is suitable for a wide range of applications. Be sure to understand
rsyslog message batch processing before you modify this option. If
you do not know what this doc here talks about, this is a good
indication that you should NOT modify the default.

Input usage
-----------
.. _param-imfile-input-maxsubmitatonce:
.. _imfile.parameter.input.maxsubmitatonce:
.. code-block:: rsyslog

   input(type="imfile" MaxSubmitAtOnce="...")

See also
--------
See also :doc:`../../configuration/modules/imfile`.
