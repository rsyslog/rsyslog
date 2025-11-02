.. _param-omhttp-tls-mycert:
.. _omhttp.parameter.input.tls-mycert:

tls.mycert
==========

.. index::
   single: omhttp; tls.mycert
   single: tls.mycert

.. summary-start

Supplies the client certificate file used for HTTPS mutual authentication.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omhttp`.

:Name: tls.mycert
:Scope: input
:Type: word
:Default: input=none
:Required?: no
:Introduced: Not specified

Description
-----------
This parameter sets the path to the SSL client certificate. Expects ``.pem`` format.

Input usage
-----------
.. _omhttp.parameter.input.tls-mycert-usage:

.. code-block:: rsyslog

   module(load="omhttp")

   action(
       type="omhttp"
       useHttps="on"
       tlsMyCert="/etc/rsyslog/certs/omhttp-client.pem"
   )

See also
--------
See also :doc:`../../configuration/modules/omhttp`.
