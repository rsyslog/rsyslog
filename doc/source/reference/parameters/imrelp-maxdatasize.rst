.. _param-imrelp-maxdatasize:
.. _imrelp.parameter.input.maxdatasize:

maxDataSize
===========

.. index::
   single: imrelp; maxDataSize
   single: maxDataSize

.. summary-start

Sets the max message size the RELP listener accepts before oversize handling.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imrelp`.

:Name: maxDataSize
:Scope: input
:Type: size_nbr
:Default: input=:doc:`global(maxMessageSize) <../../rainerscript/global>`
:Required?: no
:Introduced: Not documented

Description
-----------
Sets the max message size (in bytes) that can be received. Messages that are too
long are handled as specified in parameter :ref:`param-imrelp-oversizemode`. Note
that maxDataSize cannot be smaller than the global parameter
:doc:`global(maxMessageSize) <../../rainerscript/global>`.

Input usage
-----------
.. _param-imrelp-input-maxdatasize-usage:
.. _imrelp.parameter.input.maxdatasize-usage:

.. code-block:: rsyslog

   input(type="imrelp" port="2514" maxDataSize="10k")

See also
--------
See also :doc:`../../configuration/modules/imrelp`.
