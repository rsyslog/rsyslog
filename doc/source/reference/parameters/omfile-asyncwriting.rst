.. _param-omfile-asyncwriting:
.. _omfile.parameter.module.asyncwriting:

asyncWriting
============

.. index::
   single: omfile; asyncWriting
   single: asyncWriting

.. summary-start

If turned on, the files will be written in asynchronous mode via a
separate thread.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omfile`.

:Name: asyncWriting
:Scope: action
:Type: boolean
:Default: action=off
:Required?: no
:Introduced: at least 5.x, possibly earlier

Description
-----------

If turned on, the files will be written in asynchronous mode via a
separate thread. In that case, double buffers will be used so that
one buffer can be filled while the other buffer is being written.

The :ref:`param-omfile-flushinterval` setting is applied when
``asyncWriting`` is enabled for the action. Set ``asyncWriting="on"`` when
using ``flushInterval`` to bound how long buffered data may remain
unflushed.

Action usage
------------

.. _param-omfile-action-asyncwriting:
.. _omfile.parameter.action.asyncwriting:
.. code-block:: rsyslog

   action(type="omfile" file="/var/log/app.log" asyncWriting="on")

YAML usage
----------

.. code-block:: yaml

   actions:
     - type: omfile
       file: /var/log/app.log
       asyncWriting: "on"

Notes
-----

- Legacy documentation referred to the type as ``binary``; this maps to ``boolean``.

Legacy names (for reference)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Historic names/directives for compatibility. Do not use in new configs.

.. _omfile.parameter.legacy.omfileasyncwriting:

- $OMFileASyncWriting — maps to asyncWriting (status: legacy)

.. index::
   single: omfile; $OMFileASyncWriting
   single: $OMFileASyncWriting

See also
--------

See also :doc:`../../configuration/modules/omfile`.
