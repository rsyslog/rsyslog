.. _param-omrelp-tls-tlslib:
.. _omrelp.parameter.module.tls-tlslib:

tls.tlslib
==========

.. index::
   single: omrelp; tls.tlslib
   single: tls.tlslib

.. summary-start

Specifies which TLS library librelp uses for RELP operations.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omrelp`.

:Name: tls.tlslib
:Scope: module
:Type: string
:Default: module=none
:Required?: no
:Introduced: 8.1903.0

Description
-----------
.. versionadded:: 8.1903.0

Permits to specify the TLS library used by librelp. All RELP protocol operations are actually performed by librelp and not rsyslog itself. This value is directly passed down to librelp. Depending on librelp version and build parameters, supported TLS libraries differ (or TLS may not be supported at all). In this case rsyslog emits an error message.

Usually, the following options should be available: "openssl", "gnutls".

Note that "gnutls" is the current default for historic reasons. We actually recommend to use "openssl". It provides better error messages and accepts a wider range of certificate types.

If you have problems with the default setting, we recommend to switch to "openssl".

Module usage
------------
.. _param-omrelp-module-tls-tlslib:
.. _omrelp.parameter.module.tls-tlslib-usage:

.. code-block:: rsyslog

   module(load="omrelp" tls.tlslib="openssl")

See also
--------
See also :doc:`../../configuration/modules/omrelp`.
