.. _param-imbeats-gnutlsprioritystring:
.. _imbeats.parameter.input.gnutlsprioritystring:

gnutlsPriorityString
====================

.. meta::
   :description: GnuTLS priority string for imbeats TLS sessions.
   :keywords: rsyslog, imbeats, gnutlsprioritystring

.. index::
   single: imbeats; gnutlsPriorityString
   single: gnutlsPriorityString

.. summary-start

Override the GnuTLS priority string used by the selected TLS stream driver.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imbeats`.

:Name: gnutlsPriorityString
:Scope: input
:Type: string
:Default: input=none
:Required?: no
:Introduced: 8.2604.0

Description
-----------
Override the GnuTLS priority string used by the selected TLS stream driver.

Input usage
-----------
.. _param-imbeats-input-gnutlsprioritystring:
.. _imbeats.parameter.input.gnutlsprioritystring-usage:

.. code-block:: rsyslog

   input(type="imbeats" port="5044" gnutlsPriorityString="NORMAL")

See also
--------
See also :doc:`../../configuration/modules/imbeats`.
