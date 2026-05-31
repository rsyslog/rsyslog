.. _param-omrelp-tls-prioritystring:
.. _omrelp.parameter.input.tls-prioritystring:

TLS.PriorityString
==================

.. index::
   single: omrelp; TLS.PriorityString
   single: TLS.PriorityString

.. summary-start

Passes a custom GnuTLS priority string to control TLS parameters.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omrelp`.

:Name: TLS.PriorityString
:Scope: input
:Type: string
:Default: input=none
:Required?: no
:Introduced: at least 7.0.0, possibly earlier

Description
-----------
This parameter is passed through librelp to the selected TLS backend. With the
default GnuTLS backend, it specifies the so-called "priority string" to GnuTLS.
This string gives complete control over all crypto parameters, including
compression settings. For this reason, when ``tls.priorityString`` is specified
with GnuTLS, the ``tls.compression`` parameter has no effect and is ignored.

When :ref:`param-omrelp-tls-tlslib` is set to ``openssl``, the same rsyslog
parameter is interpreted by librelp as an OpenSSL cipher-list string, not as a
GnuTLS priority string. Use OpenSSL cipher-list syntax such as ``HIGH:!aNULL``,
or use :ref:`param-omrelp-tls-tlscfgcmd` for broader OpenSSL TLS policy such as
protocol minimums.

Full information about how to construct a GnuTLS priority string can be found in
the GnuTLS manual. At the time of writing, this information was contained in
`section 6.10 of the GnuTLS manual <http://gnutls.org/manual/html_node/Priority-Strings.html>`__.
OpenSSL cipher-list syntax is documented in the OpenSSL ``ciphers`` manual page.
**Note: this is an expert parameter.** Do not use if you do not exactly know what
you are doing.

Input usage
-----------
.. _param-omrelp-input-tls-prioritystring:
.. _omrelp.parameter.input.tls-prioritystring-usage:

.. code-block:: rsyslog

   action(type="omrelp" target="centralserv" tls.priorityString="NORMAL")

.. code-block:: rsyslog

   module(load="omrelp" tls.tlsLib="openssl")
   action(type="omrelp" target="centralserv" tls.priorityString="HIGH:!aNULL")

See also
--------
See also :doc:`../../configuration/modules/omrelp`.
