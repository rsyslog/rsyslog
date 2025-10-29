.. _param-imrelp-tls-myprivkey:
.. _imrelp.parameter.input.tls-myprivkey:

TLS.MyPrivKey
=============

.. index::
   single: imrelp; TLS.MyPrivKey
   single: TLS.MyPrivKey

.. summary-start

References the private key file that matches the configured TLS.MyCert.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imrelp`.

:Name: TLS.MyPrivKey
:Scope: input
:Type: string
:Default: input=none
:Required?: no
:Introduced: Not documented

Description
-----------
The machine private key for the configured TLS.MyCert.

Input usage
-----------
.. _param-imrelp-input-tls-myprivkey:
.. _imrelp.parameter.input.tls-myprivkey-usage:

.. code-block:: rsyslog

   input(type="imrelp" port="2514" tls="on" tls.myPrivKey="/etc/rsyslog/server.key")

See also
--------
See also :doc:`../../configuration/modules/imrelp`.
