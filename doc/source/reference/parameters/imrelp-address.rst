.. _param-imrelp-address:
.. _imrelp.parameter.input.address:

Address
=======

.. index::
   single: imrelp; Address
   single: Address

.. summary-start

Forces the RELP listener to bind to the specified local IP address or hostname.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imrelp`.

:Name: Address
:Scope: input
:Type: string
:Default: input=none
:Required?: no
:Introduced: 8.37.0

Description
-----------
Bind the RELP server to that address. If not specified, the server will be bound
to the wildcard address.

Input usage
-----------
.. _param-imrelp-input-address:
.. _imrelp.parameter.input.address-usage:

.. code-block:: rsyslog

   input(type="imrelp" port="2514" address="203.0.113.5")

See also
--------
See also :doc:`../../configuration/modules/imrelp`.
