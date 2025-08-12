.. _param-omfile-flushontxend:
.. _omfile.parameter.module.flushontxend:

flushOnTXEnd
============

.. index::
   single: omfile; flushOnTXEnd
   single: flushOnTXEnd

.. summary-start

Omfile has the capability to write output using a buffered writer.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omfile`.

:Name: flushOnTXEnd
:Scope: action
:Type: boolean
:Default: action=on
:Required?: no
:Introduced: at least 5.x, possibly earlier

Description
-----------

Omfile has the capability to write output using a buffered writer.
Disk writes are only done when the buffer is full. So if an error
happens during that write, data is potentially lost. Bear in mind that
the buffer may become full only after several hours or a rsyslog
shutdown (however a buffer flush can still be forced by sending rsyslogd
a HUP signal). In cases where this is unacceptable, set FlushOnTXEnd
to "on". Then, data is written at the end of each transaction
(for pre-v5 this means after each log message) and the usual error
recovery thus can handle write errors without data loss.
Note that this option severely reduces the effect of zip compression
and should be switched to "off" for that use case.
Also note that the default -on- is primarily an aid to preserve the
traditional syslogd behaviour.

If you are using dynamic file names (dynafiles), flushes can actually
happen more frequently. In this case, a flush can also happen when
the file name changes within a transaction.

Action usage
------------

.. _param-omfile-action-flushontxend:
.. _omfile.parameter.action.flushontxend:
.. code-block:: rsyslog

   action(type="omfile" flushOnTXEnd="...")

Notes
-----

- Legacy documentation referred to the type as ``binary``; this maps to ``boolean``.

Legacy names (for reference)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Historic names/directives for compatibility. Do not use in new configs.

.. _omfile.parameter.legacy.omfileflushontxend:

- $OMFileFlushOnTXEnd — maps to flushOnTXEnd (status: legacy)

.. index::
   single: omfile; $OMFileFlushOnTXEnd
   single: $OMFileFlushOnTXEnd

See also
--------

See also :doc:`../../configuration/modules/omfile`.
