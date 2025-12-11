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
This parameter permits to specify the so-called "priority string" to GnuTLS. This string gives complete control over all crypto parameters, including compression setting. For this reason, when the prioritystring is specified, the "tls.compression" parameter has no effect and is ignored. Full information about how to construct a priority string can be found in the GnuTLS manual. At the time of this writing, this information was contained in `section 6.10 of the GnuTLS manual <http://gnutls.org/manual/html_node/Priority-Strings.html>`__. **Note: this is an expert parameter.** Do not use if you do not exactly know what you are doing.

Input usage
-----------
.. _param-omrelp-input-tls-prioritystring:
.. _omrelp.parameter.input.tls-prioritystring-usage:

.. code-block:: rsyslog

   action(type="omrelp" target="centralserv" tls.priorityString="NORMAL")

See also
--------
See also :doc:`../../configuration/modules/omrelp`.
