.. _param-omrelp-conn-timeout:
.. _omrelp.parameter.input.conn-timeout:

Conn.Timeout
============

.. index::
   single: omrelp; Conn.Timeout
   single: Conn.Timeout

.. summary-start

Controls how long the socket connection attempt is allowed to take.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omrelp`.

:Name: Conn.Timeout
:Scope: input
:Type: integer
:Default: input=10
:Required?: no
:Introduced: at least 7.0.0, possibly earlier

Description
-----------
Timeout for the socket connection.

Input usage
-----------
.. _param-omrelp-input-conn-timeout:
.. _omrelp.parameter.input.conn-timeout-usage:

.. code-block:: rsyslog

   action(type="omrelp" target="centralserv" connTimeout="10")

See also
--------
See also :doc:`../../configuration/modules/omrelp`.
