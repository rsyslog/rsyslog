.. _param-omrelp-rebindinterval:
.. _omrelp.parameter.input.rebindinterval:

RebindInterval
==============

.. index::
   single: omrelp; RebindInterval
   single: RebindInterval

.. summary-start

Forces periodic reconnection after a configured number of transmitted messages.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omrelp`.

:Name: RebindInterval
:Scope: input
:Type: integer
:Default: input=0
:Required?: no
:Introduced: at least 7.0.0, possibly earlier

Description
-----------
Permits to specify an interval at which the current connection is broken and re-established. This setting is primarily an aid to load balancers. After the configured number of messages has been transmitted, the current connection is terminated and a new one started. This usually is perceived as a ``new connection`` by load balancers, which in turn forward messages to another physical target system.

Input usage
-----------
.. _param-omrelp-input-rebindinterval:
.. _omrelp.parameter.input.rebindinterval-usage:

.. code-block:: rsyslog

   action(type="omrelp" target="centralserv" rebindInterval="10000")

See also
--------
See also :doc:`../../configuration/modules/omrelp`.
