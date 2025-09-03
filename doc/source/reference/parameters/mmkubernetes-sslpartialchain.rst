.. _param-mmkubernetes-sslpartialchain:
.. _mmkubernetes.parameter.action.sslpartialchain:

sslpartialchain
===============

.. index::
   single: mmkubernetes; sslpartialchain
   single: sslpartialchain

.. summary-start

Enables OpenSSL ``X509_V_FLAG_PARTIAL_CHAIN`` verification.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/mmkubernetes`.

:Name: sslpartialchain
:Scope: action
:Type: boolean
:Default: off
:Required?: no
:Introduced: at least 8.x, possibly earlier

Description
-----------
This option is only available if rsyslog was built with support for OpenSSL and
only if the `X509_V_FLAG_PARTIAL_CHAIN` flag is available.  If you attempt to
set this parameter on other platforms, you will get an `INFO` level log
message.  This was done so that you could use the same configuration on
different platforms.
If `"on"`, this will set the OpenSSL certificate store flag
`X509_V_FLAG_PARTIAL_CHAIN`.   This will allow you to verify the Kubernetes API
server cert with only an intermediate CA cert in your local trust store, rather
than having to have the entire intermediate CA + root CA chain in your local
trust store.  See also `man s_client` - the `-partial_chain` flag.
If you get errors like this, you probably need to set `sslpartialchain="on"`:

.. code-block:: none

    rsyslogd: mmkubernetes: failed to connect to [https://...url...] -
    60:Peer certificate cannot be authenticated with given CA certificates

Action usage
------------
.. _param-mmkubernetes-action-sslpartialchain:
.. _mmkubernetes.parameter.action.sslpartialchain-usage:

.. code-block:: rsyslog

   action(type="mmkubernetes" sslPartialChain="on")

See also
--------
See also :doc:`../../configuration/modules/mmkubernetes`.
