.. _param-imtcp-ratelimit-burst:
.. _imtcp.parameter.input.ratelimit-burst:

RateLimit.Burst
===============

.. index::
   single: imtcp; RateLimit.Burst
   single: RateLimit.Burst

.. summary-start

Defines the maximum number of messages allowed per rate-limit interval.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imtcp`.

:Name: RateLimit.Burst
:Scope: input
:Type: integer
:Default: input=10000
:Required?: no
:Introduced: at least 5.x, possibly earlier

Description
-----------
Specifies the rate-limiting burst in number of messages. Default is ``10,000``.

Input usage
-----------
.. _param-imtcp-input-ratelimit-burst:
.. _imtcp.parameter.input.ratelimit-burst-usage:

.. code-block:: rsyslog

   input(type="imtcp" rateLimit.Burst="20000")

Notes
-----

.. include:: ratelimit-inline-note.rst

See also
--------
See also :doc:`../../configuration/modules/imtcp`.
