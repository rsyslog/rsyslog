.. _param-imtcp-ratelimit-name:
.. _imtcp.parameter.input.ratelimit-name:

RateLimit.Name
==============

.. index::
   single: imtcp; RateLimit.Name
   single: RateLimit.Name

.. summary-start

Specifies the name of the rate limiting policy to use.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imtcp`.

:Name: RateLimit.Name
:Scope: input
:Type: string
:Default: null
:Required?: no
:Introduced: 8.2602.0

Description
-----------
Specifies the name of a rate limiting policy (defined via the top-level :ref:`ratelimit` object) to assert.
If this parameter is set, the named policy is looked up and used. If the policy is not found, an error is reported.
Using a named policy allows sharing rate limits across multiple inputs or managing them centrally.

When ``global(config.reloadOnHUP="on")`` is active, an imtcp listener can switch
between unchanged named policies, or between named and unnamed limiting,
without closing established sessions.  The new listener-local limiter starts
with a fresh bucket at the transactional reload safepoint.  A new policy that
uses only ``name``, ``interval``, and ``burst`` can be added and selected by an
imtcp input in the same HUP; all imtcp inputs in that candidate share its
prepared policy bucket.  An already active simple definition can also change
when both generations reference it exclusively from imtcp.  The replacement
uses a fresh shared bucket at the transactional safepoint.  Definitions shared
with another module or action, removals, and policies using broader per-source,
file, or template settings remain restart-required.

.. warning::
   This parameter is mutually exclusive with :ref:`param-imtcp-ratelimit-interval` and :ref:`param-imtcp-ratelimit-burst`.
   If ``RateLimit.Name`` is specified, those parameters must **not** be used. Specifying both will result in a
   configuration error.

Input usage
-----------
.. _param-imtcp-input-ratelimit-name:
.. _imtcp.parameter.input.ratelimit-name-usage:

.. code-block:: rsyslog

   ratelimit(name="myLimit" interval="60" burst="100")
   input(type="imtcp" rateLimit.Name="myLimit")

See also
--------
See also :doc:`../../configuration/modules/imtcp`.
