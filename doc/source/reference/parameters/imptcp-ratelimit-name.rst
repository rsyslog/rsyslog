.. _param-imptcp-ratelimit-name:
.. _imptcp.parameter.input.ratelimit-name:

RateLimit.Name
==============

.. index::
   single: imptcp; RateLimit.Name
   single: RateLimit.Name

.. summary-start

Specifies the name of the rate limiting policy to use.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imptcp`.

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

.. warning::
   This parameter is mutually exclusive with :ref:`param-imptcp-ratelimit-interval` and :ref:`param-imptcp-ratelimit-burst`.
   If ``RateLimit.Name`` is specified, those parameters must **not** be used. Specifying both will result in a
   configuration error.

Input usage
-----------
.. _param-imptcp-input-ratelimit-name:
.. _imptcp.parameter.input.ratelimit-name-usage:

.. code-block:: rsyslog

   ratelimit(name="myLimit" interval="60" burst="100")
   input(type="imptcp" RateLimit.Name="myLimit")

See also
--------
See also :doc:`../../configuration/modules/imptcp`.
