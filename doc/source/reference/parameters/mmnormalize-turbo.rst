.. _param-mmnormalize-turbo:
.. _mmnormalize.parameter.action.turbo:

turbo
=====

.. index::
   single: mmnormalize; turbo
   single: turbo

.. summary-start

Enable TurboVM bytecode acceleration for normalization. Requires
liblognorm 2.1.0+ built with ``--enable-turbo``.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/mmnormalize`.

:Name: turbo
:Scope: action
:Type: binary
:Default: off
:Required?: no
:Introduced: 8.2602.0

Description
-----------
When enabled, mmnormalize compiles the rulebase into bytecode at
startup and uses the TurboVM engine for SIMD-accelerated
normalization. Each rsyslog worker thread gets an independent
TurboVM context, ensuring thread-safe operation without mutexes
on the hot path.

Normalized results are stored as opaque snapshots that support
zero-JSON field resolution: template access reads fields directly
from the snapshot without building a json-c object tree.

If bytecode compilation fails (e.g., unsupported parser types or
inline data exceeding 60 bytes), mmnormalize falls back to
standard ``ln_normalize()`` automatically. On HUP signal,
per-worker TurboVM contexts are rebuilt.

**Requirements:**

- liblognorm >= 2.1.0 with ``--enable-turbo``
- The ``lognorm-features.h`` header must define ``LOGNORM_TURBO_SUPPORTED``
- rsyslog detects this automatically at ``./configure`` time

Action usage
------------
.. _param-mmnormalize-action-turbo:
.. _mmnormalize.parameter.action.turbo-usage:

.. code-block:: rsyslog

   action(type="mmnormalize"
          rulebase="/etc/rsyslog.d/rules.rb"
          turbo="on")

See also
--------
See also :doc:`../../configuration/modules/mmnormalize`.
