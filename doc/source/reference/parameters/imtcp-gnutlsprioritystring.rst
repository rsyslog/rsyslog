.. _param-imtcp-gnutlsprioritystring:
.. _imtcp.parameter.module.gnutlsprioritystring:

gnutlsPriorityString
====================

.. index::
   single: imtcp; gnutlsPriorityString
   single: gnutlsPriorityString

.. summary-start

Provides driver-specific TLS configuration via a priority string.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imtcp`.

:Name: gnutlsPriorityString
:Scope: module
:Type: string (see :doc:`../../rainerscript/constant_strings`)
:Default: module=none
:Required?: no
:Introduced: 8.29.0

Description
-----------
The "gnutls priority string" parameter in rsyslog offers enhanced
customization for secure communications, allowing detailed configuration
of TLS driver properties. This includes specifying handshake algorithms
and other settings for GnuTLS, as well as implementing OpenSSL
configuration commands. Initially developed for GnuTLS, the "gnutls
priority string" has evolved since version v8.1905.0 to also support
OpenSSL, broadening its application and utility in network security
configurations. This update signifies a key advancement in rsyslog's
capabilities, making the "gnutls priority string" an essential
feature for advanced TLS configuration.
.. versionadded:: 8.29.0

**Configuring Driver-Specific Properties**

This configuration string is used to set properties specific to different drivers. Originally designed for the GnuTLS driver, it has been extended to support OpenSSL configuration commands from version v8.1905.0 onwards.

**GNUTLS Configuration**

In GNUTLS, this setting determines the handshake algorithms and options for the TLS session. It's designed to allow user overrides of the library's default settings. If you leave this parameter unset (NULL), the system will revert to the default settings. For more detailed information on priority strings in GNUTLS, you can refer to the GnuTLS Priority Strings Documentation available at [GnuTLS Website](https://gnutls.org/manual/html_node/Priority-Strings.html).

**OpenSSL Configuration**

This feature is compatible with OpenSSL Version 1.0.2 and above. It enables the passing of configuration commands to the OpenSSL library. You can find a comprehensive list of commands and their acceptable values in the OpenSSL Documentation, accessible at [OpenSSL Documentation](https://docs.openssl.org/1.0.2/man3/SSL_CONF_cmd/).

**General Configuration Guidelines**

The configuration can be formatted as a single line or across multiple lines. Each command within the configuration is separated by a linefeed (``\n``). To differentiate between a command and its corresponding value, use an equal sign (``=``). Below are some examples to guide you in formatting these commands.

Example 1
---------
This will allow all protocols except for SSLv2 and SSLv3:

.. code-block:: none

   gnutlsPriorityString="Protocol=ALL,-SSLv2,-SSLv3"

Example 2
---------
This will allow all protocols except for SSLv2, SSLv3 and TLSv1.
It will also set the minimum protocol to TLSv1.2

.. code-block:: none

   gnutlsPriorityString="Protocol=ALL,-SSLv2,-SSLv3,-TLSv1
   MinProtocol=TLSv1.2"

The same-named input parameter can override this module setting.


Module usage
------------
.. _param-imtcp-module-gnutlsprioritystring:
.. _imtcp.parameter.module.gnutlsprioritystring-usage:

.. code-block:: rsyslog

   module(load="imtcp" gnutlsPriorityString="Protocol=ALL,-SSLv2,-SSLv3")

See also
--------
See also :doc:`../../configuration/modules/imtcp`.

