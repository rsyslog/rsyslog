.. _param-imrelp-tls-mycert:
.. _imrelp.parameter.input.tls-mycert:

tls.myCert
==========

.. index::
   single: imrelp; tls.myCert
   single: tls.myCert

.. summary-start

Points rsyslog to the server certificate presented during TLS handshakes.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imrelp`.

:Name: tls.myCert
:Scope: input
:Type: string
:Default: input=none
:Required?: no
:Introduced: Not documented

Description
-----------
The machine certificate that is being used for TLS communication.

Input usage
-----------
.. _param-imrelp-input-tls-mycert-usage:
.. _imrelp.parameter.input.tls-mycert-usage:

.. code-block:: rsyslog

   input(type="imrelp" port="2514" tls="on" tls.myCert="/etc/rsyslog/server.pem")

See also
--------
See also :doc:`../../configuration/modules/imrelp`.
