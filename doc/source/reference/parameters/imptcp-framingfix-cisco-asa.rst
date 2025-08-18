.. _param-imptcp-framingfix-cisco-asa:
.. _imptcp.parameter.input.framingfix-cisco-asa:

Framingfix.cisco.asa
====================

.. index::
   single: imptcp; Framingfix.cisco.asa
   single: Framingfix.cisco.asa

.. summary-start

Ignores a leading space after a line feed to work around Cisco ASA framing issues.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imptcp`.

:Name: Framingfix.cisco.asa
:Scope: input
:Type: boolean
:Default: input=off
:Required?: no
:Introduced: at least 5.x, possibly earlier

Description
-----------
Cisco very occasionally sends a space after a line feed, which thrashes framing
if not taken special care of. When this parameter is set to "on", we permit
space *in front of the next frame* and ignore it.

Input usage
-----------
.. _param-imptcp-input-framingfix-cisco-asa:
.. _imptcp.parameter.input.framingfix-cisco-asa-usage:

.. code-block:: rsyslog

   input(type="imptcp" framingfix.cisco.asa="...")

See also
--------
See also :doc:`../../configuration/modules/imptcp`.
