.. _param-imgssapi-inputgssserverkeepalive:
.. _imgssapi.parameter.input.inputgssserverkeepalive:

InputGSSServerKeepAlive
=======================

.. index::
   single: imgssapi; InputGSSServerKeepAlive
   single: InputGSSServerKeepAlive

.. summary-start

Enables TCP keep-alive handling for GSS-protected connections.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imgssapi`.

:Name: InputGSSServerKeepAlive
:Scope: input
:Type: boolean
:Default: 0
:Required?: no
:Introduced: 8.5.0

Description
-----------
Enables or disables keep-alive packets at the TCP socket layer.

Input usage
-----------
.. _imgssapi.parameter.input.inputgssserverkeepalive-usage:

.. code-block:: rsyslog

   module(load="imgssapi")
   $inputGSSServerKeepAlive on

Legacy names (for reference)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. _imgssapi.parameter.legacy.inputgssserverkeepalive:

- $inputGSSServerKeepAlive — maps to InputGSSServerKeepAlive (status: legacy)

.. index::
   single: imgssapi; $inputGSSServerKeepAlive
   single: $inputGSSServerKeepAlive

See also
--------
See also :doc:`../../configuration/modules/imgssapi`.
