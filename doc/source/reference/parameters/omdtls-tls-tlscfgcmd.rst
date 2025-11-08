.. _param-omdtls-tls-tlscfgcmd:
.. _omdtls.parameter.input.tls-tlscfgcmd:

tls.tlscfgcmd
=============

.. index::
   single: omdtls; tls.tlscfgcmd
   single: tls.tlscfgcmd

.. summary-start

Passes additional OpenSSL configuration commands to fine-tune DTLS behavior.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omdtls`.

:Name: tls.tlscfgcmd
:Scope: input
:Type: string
:Default: input=none
:Required?: no
:Introduced: v8.2402.0

Description
-----------
Used to pass additional OpenSSL configuration commands. This can be used to
fine-tune the OpenSSL settings by passing configuration commands to the OpenSSL
library. OpenSSL Version 1.0.2 or higher is required for this feature. A list of
possible commands and their valid values can be found in the documentation:
https://docs.openssl.org/1.0.2/man3/SSL_CONF_cmd/

The setting can be single or multiline, each configuration command is separated
by linefeed (``\n``). Command and value are separated by an equal sign (``=``).
Here are a few samples:

Example 1
~~~~~~~~~
This will allow all protocols except for SSLv2 and SSLv3:

.. code-block:: none

   tls.tlscfgcmd="Protocol=ALL,-SSLv2,-SSLv3"

Example 2
~~~~~~~~~
This will allow all protocols except for SSLv2, SSLv3 and TLSv1. It will also
set the minimum protocol to TLSv1.2

.. code-block:: none

   tls.tlscfgcmd="Protocol=ALL,-SSLv2,-SSLv3,-TLSv1\nMinProtocol=TLSv1.2"

Input usage
-----------
.. _param-omdtls-input-tls-tlscfgcmd:
.. _omdtls.parameter.input.tls-tlscfgcmd-usage:

.. code-block:: rsyslog

   action(type="omdtls"
          target="192.0.2.1"
          tls.tlsCfgCmd="Protocol=ALL,-SSLv2,-SSLv3")

See also
--------
See also :doc:`../../configuration/modules/omdtls`.
