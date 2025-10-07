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
:Default: host
:Required?: no
:Introduced: 3.11.5

Description
-----------
Specifies the service name for the GSS server, used to form the Service
Principal Name (SPN). This can be a simple name like ``host`` or a
compound name such as ``host/rsyslog.example.com``. If not specified, it
defaults to ``host``.

Input usage
-----------
.. _imgssapi.parameter.input.inputgssserverservicename-usage:

.. code-block:: rsyslog

   module(load="imgssapi")
   $inputGSSServerServiceName "host/rsyslog.example.com"

Legacy names (for reference)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. _imgssapi.parameter.legacy.inputgssserverservicename:

- $inputGSSServerServiceName — maps to InputGSSServerServiceName (status: legacy)

.. index::
   single: imgssapi; $inputGSSServerServiceName
   single: $inputGSSServerServiceName

See also
--------
See also :doc:`../../configuration/modules/imgssapi`.
