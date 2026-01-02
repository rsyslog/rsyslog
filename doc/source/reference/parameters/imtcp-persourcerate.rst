.. _param-imtcp-persourcerate:
.. _imtcp.parameter.input.persourcerate:

perSourceRate
=============

.. index::
   single: imtcp; perSourceRate

.. summary-start

Enables per-source rate limiting using the named ratelimit() policy.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imtcp`.

:Name: perSourceRate
:Scope: input
:Type: boolean
:Default: off
:Required?: no
:Introduced: 8.2602.0

Description
-----------
When set to ``on``, per-source rate limiting is evaluated for each message
using the ratelimit() policy referenced by ``RateLimit.Name``. The per-source
policy must be provided via ``perSourcePolicy`` on the ratelimit() object.

.. warning::
   ``perSourceRate`` requires ``RateLimit.Name`` to be set. Without a named
   ratelimit() policy, configuration loading fails.

Input usage
-----------
.. _param-imtcp-input-persourcerate:

.. code-block:: rsyslog

   ratelimit(name="per_source" perSource="on" perSourcePolicy="/etc/rsyslog/imtcp-ratelimits.yaml")
   input(type="imtcp" perSourceRate="on" rateLimit.Name="per_source")

See also
--------
See also :doc:`../../configuration/modules/imtcp`.
