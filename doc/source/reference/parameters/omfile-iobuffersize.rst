.. _param-omfile-iobuffersize:
.. _omfile.parameter.module.iobuffersize:

ioBufferSize
============

.. index::
   single: omfile; ioBufferSize
   single: ioBufferSize

.. summary-start

Size of the buffer used to writing output data.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omfile`.

:Name: ioBufferSize
:Scope: action
:Type: size
:Default: action=4 KiB
:Required?: no
:Introduced: at least 5.x, possibly earlier

Description
-----------

Size of the buffer used to writing output data. The larger the
buffer, the potentially better performance is. The default of 4k is
quite conservative, it is useful to go up to 64k, and 128k if you
used gzip compression (then, even higher sizes may make sense)

Action usage
------------

.. _param-omfile-action-iobuffersize:
.. _omfile.parameter.action.iobuffersize:
.. code-block:: rsyslog

   action(type="omfile" ioBufferSize="...")

Legacy names (for reference)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Historic names/directives for compatibility. Do not use in new configs.

.. _omfile.parameter.legacy.omfileiobuffersize:

- $OMFileIOBufferSize — maps to ioBufferSize (status: legacy)

.. index::
   single: omfile; $OMFileIOBufferSize
   single: $OMFileIOBufferSize

See also
--------

See also :doc:`../../configuration/modules/omfile`.
