.. _param-omrelp-timeout:
.. _omrelp.parameter.input.timeout:

Timeout
=======

.. index::
   single: omrelp; Timeout
   single: Timeout

.. summary-start

Sets the RELP session timeout before a connection is considered dead.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omrelp`.

:Name: Timeout
:Scope: input
:Type: integer
:Default: input=90
:Required?: no
:Introduced: at least 7.0.0, possibly earlier

Description
-----------
Timeout for relp sessions. If set too low, valid sessions may be considered dead and tried to recover.

Input usage
-----------
.. _param-omrelp-input-timeout:
.. _omrelp.parameter.input.timeout-usage:

.. code-block:: rsyslog

   action(type="omrelp" target="centralserv" timeout="120")

See also
--------
See also :doc:`../../configuration/modules/omrelp`.
