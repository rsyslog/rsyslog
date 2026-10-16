.. _param-imtcp-ratelimit-name:
.. _imtcp.parameter.input.ratelimit-name:

.. meta::
   :description: Select a shared named imtcp rate policy and understand its bounded live-reload contract.
   :keywords: rsyslog, imtcp, ratelimit, named policy, transactional reload

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
Using a named policy lets multiple inputs reference a centrally managed policy definition.

With ``global(config.reloadOnHUP="on")``, existing imtcp listeners can select,
add, change or remove simple named policies containing only ``name``,
``interval``, ``burst`` and ``severity``. A definition can change only when
both active and candidate configurations reference it exclusively from imtcp.
Removal must switch every remaining consumer away in the same candidate.
For this supported simple-policy subset, the definition is shared centrally,
but each listener has an independent local budget. Unchanged policies retain
their configuration owner even while temporarily unused; continuously bound
listeners keep their local budgets across unrelated updates. Changed or new
policies give each consumer a fresh local limiter at the transactional safepoint,
backed by one common configuration owner. Unbinding and rebinding an unchanged
policy starts a fresh local budget; established connections remain open.
Definitions shared with actions or other modules, complex policies, malformed
values and missing definitions remain restart-required or rejected. This does
not enable general configuration reload or listener topology changes.

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
