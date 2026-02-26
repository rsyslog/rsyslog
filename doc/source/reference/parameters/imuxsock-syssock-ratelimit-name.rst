.. index:: ! imuxsock; SysSock.RateLimit.Name

.. _param-imuxsock-syssock-ratelimit-name:

SysSock.RateLimit.Name
======================

.. summary-start

**Default:** none

**Type:** string

**Description:**

Sets the name of the rate limit to use for the system log socket. The name refers to a
:doc:`global rate limit object <../../rainerscript/configuration_objects/ratelimit>` defined in
the configuration.

**Note:** This parameter is mutually exclusive with ``syssock.ratelimit.interval`` and
``syssock.ratelimit.burst``. If ``syssock.ratelimit.name`` is specified, local limits cannot be
defined and any attempt to do so will result in an error (and the named rate limit will be used).

.. versionadded:: 8.2602.0

.. summary-end
