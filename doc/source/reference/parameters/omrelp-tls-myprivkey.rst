.. _param-omrelp-tls-myprivkey:
.. _omrelp.parameter.input.tls-myprivkey:

TLS.MyPrivKey
=============

.. index::
   single: omrelp; TLS.MyPrivKey
   single: TLS.MyPrivKey

.. summary-start

Specifies the path to the machine's private key for TLS.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omrelp`.

:Name: TLS.MyPrivKey
:Scope: input
:Type: string
:Default: input=none
:Required?: no
:Introduced: at least 7.0.0, possibly earlier

Description
-----------
The machine private key.

Input usage
-----------
.. _param-omrelp-input-tls-myprivkey:
.. _omrelp.parameter.input.tls-myprivkey-usage:

.. code-block:: rsyslog

   action(type="omrelp" target="centralserv" tls.myPrivKey="/path/to/key.pem")

See also
--------
See also :doc:`../../configuration/modules/omrelp`.
