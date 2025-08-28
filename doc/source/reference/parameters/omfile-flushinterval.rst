.. _param-omfile-flushinterval:
.. _omfile.parameter.module.flushinterval:

flushInterval
=============

.. index::
   single: omfile; flushInterval
   single: flushInterval

.. summary-start

Defines, in seconds, the interval after which unwritten data is
flushed.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omfile`.

:Name: flushInterval
:Scope: action
:Type: integer
:Default: action=1
:Required?: no
:Introduced: at least 5.x, possibly earlier

Description
-----------

Defines, in seconds, the interval after which unwritten data is
flushed.

Action usage
------------

.. _param-omfile-action-flushinterval:
.. _omfile.parameter.action.flushinterval:
.. code-block:: rsyslog

   action(type="omfile" flushInterval="...")

Legacy names (for reference)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Historic names/directives for compatibility. Do not use in new configs.

.. _omfile.parameter.legacy.omfileflushinterval:

- $OMFileFlushInterval — maps to flushInterval (status: legacy)

.. index::
   single: omfile; $OMFileFlushInterval
   single: $OMFileFlushInterval

See also
--------

See also :doc:`../../configuration/modules/omfile`.
