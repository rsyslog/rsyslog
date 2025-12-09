.. _param-omrelp-tls-mycert:
.. _omrelp.parameter.input.tls-mycert:

TLS.MyCert
==========

.. index::
   single: omrelp; TLS.MyCert
   single: TLS.MyCert

.. summary-start

Provides the path to the machine's public certificate.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omrelp`.

:Name: TLS.MyCert
:Scope: input
:Type: string
:Default: input=none
:Required?: no
:Introduced: at least 7.0.0, possibly earlier

Description
-----------
The machine public certificate.

Input usage
-----------
.. _param-omrelp-input-tls-mycert:
.. _omrelp.parameter.input.tls-mycert-usage:

.. code-block:: rsyslog

   action(type="omrelp" target="centralserv" tls.myCert="/path/to/cert.pem")

See also
--------
See also :doc:`../../configuration/modules/omrelp`.
