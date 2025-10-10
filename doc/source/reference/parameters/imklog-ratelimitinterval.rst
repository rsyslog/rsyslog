.. _param-imklog-ratelimitinterval:
.. _imklog.parameter.module.ratelimitinterval:

RatelimitInterval
=================

.. index::
   single: imklog; RatelimitInterval
   single: RatelimitInterval

.. summary-start

Sets the interval window for the imklog rate limiter in seconds.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imklog`.

:Name: RatelimitInterval
:Scope: module
:Type: integer
:Default: 0
:Required?: no
:Introduced: 8.35.0

Description
-----------
.. versionadded:: 8.35.0

   Added support for imklog's built-in rate limiter.

The rate-limiting interval in seconds. Value 0 turns off rate limiting.
Set it to a number of seconds (5 recommended) to activate rate-limiting.

Module usage
------------
.. _param-imklog-module-ratelimitinterval:
.. _imklog.parameter.module.ratelimitinterval-usage:

.. code-block:: rsyslog

   module(load="imklog" rateLimitInterval="5")

See also
--------
See also :doc:`../../configuration/modules/imklog`.
