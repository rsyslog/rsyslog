.. SPDX-License-Identifier: Apache-2.0

.. index:: ! imrelp; RateLimit.Name

.. _param-imrelp-ratelimit-name:

RateLimit.Name
==============

.. summary-start

**Default:** none

**Type:** string

**Description:**

Sets the name of the rate limit to use. All listeners (even across different modules) that
reference the same name share the same rate limiting configuration and per-source tracking
state. Each listener still maintains its own basic message counters so that independent input
streams are rate-limited individually.
The name refers to a :doc:`global rate limit object
<../../rainerscript/configuration_objects/ratelimit>` defined in the configuration.

**Note:** This parameter is mutually exclusive with ``ratelimit.interval`` and
``ratelimit.burst``. If ``ratelimit.name`` is specified, local per-listener limits cannot be
defined and any attempt to do so will result in an error (and the named rate limit will be used).

.. versionadded:: 8.2604.0

.. summary-end
