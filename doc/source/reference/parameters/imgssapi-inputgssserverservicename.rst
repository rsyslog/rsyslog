.. _param-imgssapi-inputgssserverservicename:
.. _imgssapi.parameter.input.inputgssserverservicename:

InputGSSServerServiceName
=========================

.. index::
   single: imgssapi; InputGSSServerServiceName
   single: InputGSSServerServiceName

.. summary-start

Sets the Kerberos service principal name used by the GSS server.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imgssapi`.

:Name: InputGSSServerServiceName
:Scope: input
:Type: word
:Default: input=none
:Required?: no
:Introduced: Not specified

Description
-----------
The service name to use for the GSS server.

Input usage
-----------
.. _param-imgssapi-input-inputgssserverservicename:
.. _imgssapi.parameter.input.inputgssserverservicename-usage:

.. code-block:: rsyslog

   module(load="imgssapi")
   $InputGSSServerServiceName "host/rsyslog.example.com"

Legacy names (for reference)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Historic names/directives for compatibility. Do not use in new configs.

.. _imgssapi.parameter.legacy.inputgssserverservicename:

- $InputGSSServerServiceName — maps to InputGSSServerServiceName (status: legacy)

.. index::
   single: imgssapi; $InputGSSServerServiceName
   single: $InputGSSServerServiceName

See also
--------
See also :doc:`../../configuration/modules/imgssapi`.
