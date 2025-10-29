.. _param-imrelp-tls-myprivkey:
.. _imrelp.parameter.input.tls-myprivkey:

tls.myPrivKey
=============

.. index::
   single: imrelp; tls.myPrivKey
   single: tls.myPrivKey

.. summary-start

References the private key file that matches the configured tls.myCert.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imrelp`.

:Name: tls.myPrivKey
:Scope: input
:Type: string
:Default: input=none
:Required?: no
:Introduced: Not documented

Description
-----------
The machine private key for the configured tls.myCert.

Input usage
-----------
.. _param-imrelp-input-tls-myprivkey:
.. _imrelp.parameter.input.tls-myprivkey-usage:

.. code-block:: rsyslog

   input(type="imrelp" port="2514" tls="on" tls.myPrivKey="/etc/rsyslog/server.key")

See also
--------
See also :doc:`../../configuration/modules/imrelp`.
