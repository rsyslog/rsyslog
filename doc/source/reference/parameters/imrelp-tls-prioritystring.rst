.. _param-imrelp-tls-prioritystring:
.. _imrelp.parameter.input.tls-prioritystring:

tls.priorityString
==================

.. index::
   single: imrelp; tls.priorityString
   single: tls.priorityString

.. summary-start

Passes a custom GnuTLS priority string to fine-tune cryptographic parameters.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imrelp`.

:Name: tls.priorityString
:Scope: input
:Type: string
:Default: input=none
:Required?: no
:Introduced: Not documented

Description
-----------
This parameter is passed through librelp to the selected TLS backend. With the
default GnuTLS backend, it is a GnuTLS priority string and gives complete
control over crypto parameters, including compression settings. For this reason,
when ``tls.priorityString`` is specified with GnuTLS, the
:ref:`param-imrelp-tls-compression` parameter has no effect and is ignored.

When :ref:`param-imrelp-tls-tlslib` is set to ``openssl``, the same rsyslog
parameter is interpreted by librelp as an OpenSSL cipher-list string, not as a
GnuTLS priority string. Use OpenSSL cipher-list syntax such as ``HIGH:!aNULL``,
or use :ref:`param-imrelp-tls-tlscfgcmd` for broader OpenSSL TLS policy such as
protocol minimums.

Full information about how to construct a GnuTLS priority string can be found in
the GnuTLS manual. At the time of writing, this information was contained in
`section 6.10 of the GnuTLS manual <http://gnutls.org/manual/html_node/Priority-Strings.html>`_.
OpenSSL cipher-list syntax is documented in the OpenSSL ``ciphers`` manual page.

**Note: this is an expert parameter.** Do not use if you do not exactly know
what you are doing.

Input usage
-----------
.. _param-imrelp-input-tls-prioritystring-usage:
.. _imrelp.parameter.input.tls-prioritystring-usage:

.. code-block:: rsyslog

   input(type="imrelp" port="2514" tls="on" tls.priorityString="NONE:+COMP-DEFLATE")

.. code-block:: rsyslog

   module(load="imrelp" tls.tlsLib="openssl")
   input(type="imrelp" port="2514" tls="on" tls.priorityString="HIGH:!aNULL")

See also
--------
See also :doc:`../../configuration/modules/imrelp`.
