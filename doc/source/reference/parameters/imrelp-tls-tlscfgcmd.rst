.. _param-imrelp-tls-tlscfgcmd:
.. _imrelp.parameter.input.tls-tlscfgcmd:

tls.tlsCfgCmd
=============

.. index::
   single: imrelp; tls.tlsCfgCmd
   single: tls.tlsCfgCmd

.. summary-start

Forwards SSL_CONF-style configuration commands to OpenSSL when using the openssl backend.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imrelp`.

:Name: tls.tlsCfgCmd
:Scope: input
:Type: string
:Default: input=none
:Required?: no
:Introduced: 8.2001.0

Description
-----------
The setting can be used if :ref:`param-imrelp-tls-tlslib` is set to "openssl" to pass configuration
commands to the openssl library. OpenSSL Version 1.0.2 or higher is required for
this feature. A list of possible commands and their valid values can be found in
the documentation: https://docs.openssl.org/1.0.2/man3/SSL_CONF_cmd/

The setting can be single or multiline, each configuration command is separated
by linefeed (\n). Command and value are separated by equal sign (=). Here are a
few samples:

Example 1
---------

This will allow all protocols except for SSLv2 and SSLv3:

.. code-block:: none

   tls.tlsCfgCmd="Protocol=ALL,-SSLv2,-SSLv3"

Example 2
---------

This will allow all protocols except for SSLv2, SSLv3 and TLSv1.
It will also set the minimum protocol to TLSv1.2.

.. code-block:: none

   tls.tlsCfgCmd="Protocol=ALL,-SSLv2,-SSLv3,-TLSv1
   MinProtocol=TLSv1.2"

Input usage
-----------
.. _param-imrelp-input-tls-tlscfgcmd:
.. _imrelp.parameter.input.tls-tlscfgcmd-usage:

.. code-block:: rsyslog

   input(type="imrelp" port="2514" tls="on"
        tls.tlsLib="openssl"
        tls.tlsCfgCmd="Protocol=ALL,-SSLv2,-SSLv3")

See also
--------
See also :doc:`../../configuration/modules/imrelp`.
