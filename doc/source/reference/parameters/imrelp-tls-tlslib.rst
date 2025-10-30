.. _param-imrelp-tls-tlslib:
.. _imrelp.parameter.module.tls-tlslib:

tls.tlsLib
==========

.. index::
   single: imrelp; tls.tlsLib
   single: tls.tlsLib

.. summary-start

Selects the TLS backend library that librelp should use for RELP operations.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imrelp`.

:Name: tls.tlsLib
:Scope: module
:Type: word
:Default: module=none
:Required?: no
:Introduced: 8.1903.0

Description
-----------
Permits to specify the TLS library used by librelp. All RELP protocol operations
are actually performed by librelp and not rsyslog itself. The value specified is
directly passed down to librelp. Depending on librelp version and build
parameters, supported TLS libraries differ (or TLS may not be supported at all).
In this case rsyslog emits an error message.

Usually, the following options should be available: "openssl", "gnutls".

Note that "gnutls" is the current default for historic reasons. We actually
recommend to use "openssl". It provides better error messages and accepts a
wider range of certificate types.

If you have problems with the default setting, we recommend to switch to
"openssl".

Module usage
------------
.. _param-imrelp-module-tls-tlslib-usage:
.. _imrelp.parameter.module.tls-tlslib-usage:

.. code-block:: rsyslog

   module(load="imrelp" tls.tlsLib="openssl")

See also
--------
See also :doc:`../../configuration/modules/imrelp`.
