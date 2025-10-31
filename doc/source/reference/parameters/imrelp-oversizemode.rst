.. _param-imrelp-oversizemode:
.. _imrelp.parameter.input.oversizemode:

oversizeMode
============

.. index::
   single: imrelp; oversizeMode
   single: oversizeMode

.. summary-start

Determines how messages larger than MaxDataSize are handled by the listener.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imrelp`.

:Name: oversizeMode
:Scope: input
:Type: string
:Default: input=truncate
:Required?: no
:Introduced: 8.35.0

Description
-----------
This parameter specifies how messages that are too long will be handled. For
this parameter the length of the parameter :ref:`param-imrelp-maxdatasize` is
used.

- truncate: Messages will be truncated to the maximum message size.
- abort: This is the behaviour until version 8.35.0. Upon receiving a message
  that is too long imrelp will abort.
- accept: Messages will be accepted even if they are too long and an error
  message will be output. Using this option does have associated risks.

Input usage
-----------
.. _param-imrelp-input-oversizemode-usage:
.. _imrelp.parameter.input.oversizemode-usage:

.. code-block:: rsyslog

   input(type="imrelp" port="2514" maxDataSize="10k" oversizeMode="truncate")

See also
--------
See also :doc:`../../configuration/modules/imrelp`.
