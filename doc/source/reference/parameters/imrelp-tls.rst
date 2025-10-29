.. _param-imrelp-tls:
.. _imrelp.parameter.input.tls:

TLS
===

.. index::
   single: imrelp; TLS
   single: TLS (imrelp)

.. summary-start

Enables TLS encryption for RELP connections handled by this listener.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imrelp`.

:Name: TLS
:Scope: input
:Type: boolean
:Default: input=off
:Required?: no
:Introduced: Not documented

Description
-----------
If set to "on", the RELP connection will be encrypted by TLS, so that the data
is protected against observers. Please note that both the client and the server
must have set TLS to either "on" or "off". Other combinations lead to
unpredictable results.

*Attention when using GnuTLS 2.10.x or older*

Versions older than GnuTLS 2.10.x may cause a crash (Segfault) under certain
circumstances. Most likely when an imrelp inputs and an omrelp output is
configured. The crash may happen when you are receiving/sending messages at the
same time. Upgrade to a newer version like GnuTLS 2.12.21 to solve the problem.

Input usage
-----------
.. _param-imrelp-input-tls:
.. _imrelp.parameter.input.tls-usage:

.. code-block:: rsyslog

   input(type="imrelp" port="2514" tls="on")

See also
--------
See also :doc:`../../configuration/modules/imrelp`.
