.. _param-imdtls-tls-tlscfgcmd:
.. _imdtls.parameter.input.tls-tlscfgcmd:

tls.tlscfgcmd
=============

.. index::
   single: imdtls; tls.tlscfgcmd
   single: tls.tlscfgcmd

.. summary-start


Passes additional OpenSSL configuration commands to fine-tune DTLS handling.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imdtls`.

:Name: tls.tlscfgcmd
:Scope: input
:Type: string
:Default: none
:Required?: no
:Introduced: v8.2402.0

Description
-----------
Used to pass additional OpenSSL configuration commands. This can be used to fine-tune the OpenSSL settings by passing configuration commands to the OpenSSL library. OpenSSL version 1.0.2 or higher is required for this feature. A list of possible commands and their valid values can be found in the `documentation <https://www.openssl.org/docs/man-latest/man3/SSL_CONF_cmd.html>`_.

The setting can be single or multiline, each configuration command is separated by linefeed (``\n``). Command and value are separated by equal sign (``=``).

Examples
~~~~~~~~
This will allow all protocols except for SSLv2 and SSLv3:

.. code-block:: none

   tls.tlscfgcmd="Protocol=ALL,-SSLv2,-SSLv3"

This will allow all protocols except for SSLv2, SSLv3 and TLSv1 and will also set the minimum protocol to TLSv1.2:

.. code-block:: none

   tls.tlscfgcmd="Protocol=ALL,-SSLv2,-SSLv3,-TLSv1\nMinProtocol=TLSv1.2"

Input usage
-----------
.. _param-imdtls-input-tls-tlscfgcmd:
.. _imdtls.parameter.input.tls-tlscfgcmd-usage:

.. code-block:: rsyslog

   module(load="imdtls")
   input(type="imdtls" tls.tlscfgcmd="Protocol=ALL,-SSLv2,-SSLv3")

See also
--------
See also :doc:`../../configuration/modules/imdtls`.
