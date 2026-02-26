.. index:: ! imjournal; RateLimit.Name

.. _param-imjournal-ratelimit-name:

RateLimit.Name
==============

.. summary-start

**Default:** none

**Type:** string

**Description:**

Sets the name of the rate limit to use. The name refers to a :doc:`global rate limit object
<../../rainerscript/configuration_objects/ratelimit>` defined in the configuration.

**Note:** This parameter is mutually exclusive with ``ratelimit.interval`` and
``ratelimit.burst``. If ``ratelimit.name`` is specified, inline rate limit parameters cannot be
defined and any attempt to do so will result in an error (and the named rate limit will be used).

.. versionadded:: 8.2602.0

.. summary-end
