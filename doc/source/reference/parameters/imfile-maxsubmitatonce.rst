.. _param-imfile-maxsubmitatonce:
.. _imfile.parameter.input.maxsubmitatonce:
.. _imfile.parameter.maxsubmitatonce:

MaxSubmitAtOnce
===============

.. index::
   single: imfile; MaxSubmitAtOnce
   single: MaxSubmitAtOnce

.. summary-start

Sets the maximum batch size that imfile submits at once.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imfile`.

:Name: MaxSubmitAtOnce
:Scope: input
:Type: integer
:Default: 1024
:Required?: no
:Introduced: at least 8.x, possibly earlier

Description
-----------
An expert option that defines the maximum number of messages a single
input batch may contain. The default of ``1024`` suits most
applications. Modify only if familiar with rsyslog's batch processing
semantics.

Input usage
-----------
.. _param-imfile-input-maxsubmitatonce:
.. _imfile.parameter.input.maxsubmitatonce-usage:

.. code-block:: rsyslog

   input(type="imfile"
         File="/var/log/example.log"
         Tag="example"
         MaxSubmitAtOnce="1024")

See also
--------
See also :doc:`../../configuration/modules/imfile`.
