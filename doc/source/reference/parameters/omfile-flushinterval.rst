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

This parameter is applied when :ref:`param-omfile-asyncwriting` is enabled
for the action. Use ``flushInterval`` together with ``asyncWriting="on"``
when :ref:`param-omfile-flushontxend` is disabled, for example to keep gzip
compression effective while still bounding how long buffered data may remain
unflushed.

Action usage
------------

.. _param-omfile-action-flushinterval:
.. _omfile.parameter.action.flushinterval:
.. code-block:: rsyslog

   action(type="omfile" file="/var/log/app.log"
          asyncWriting="on" flushOnTXEnd="off" flushInterval=1)

YAML usage
----------

.. code-block:: yaml

   actions:
     - type: omfile
       file: /var/log/app.log
       asyncWriting: "on"
       flushOnTXEnd: "off"
       flushInterval: 1

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
