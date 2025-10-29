.. _param-imrelp-tls-prioritystring:
.. _imrelp.parameter.input.tls-prioritystring:

TLS.PriorityString
==================

.. index::
   single: imrelp; TLS.PriorityString
   single: TLS.PriorityString

.. summary-start

Passes a custom GnuTLS priority string to fine-tune cryptographic parameters.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imrelp`.

:Name: TLS.PriorityString
:Scope: input
:Type: string
:Default: input=none
:Required?: no
:Introduced: Not documented

Description
-----------
This parameter allows passing the so-called "priority string" to GnuTLS. This
string gives complete control over all crypto parameters, including compression
settings. For this reason, when the prioritystring is specified, the
"tls.compression" parameter has no effect and is ignored.

Full information about how to construct a priority string can be found in the
GnuTLS manual. At the time of writing, this information was contained in `section
6.10 of the GnuTLS manual <http://gnutls.org/manual/html_node/Priority-Strings.html>`_.

**Note: this is an expert parameter.** Do not use if you do not exactly know
what you are doing.

Input usage
-----------
.. _param-imrelp-input-tls-prioritystring:
.. _imrelp.parameter.input.tls-prioritystring-usage:

.. code-block:: rsyslog

   input(type="imrelp" port="2514" tls="on" tls.priorityString="NONE:+COMP-DEFLATE")

See also
--------
See also :doc:`../../configuration/modules/imrelp`.
