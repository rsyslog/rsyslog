.. _param-omfile-veryrobustzip:
.. _omfile.parameter.module.veryrobustzip:

veryRobustZip
=============

.. index::
   single: omfile; veryRobustZip
   single: veryRobustZip

.. summary-start

If *zipLevel* is greater than 0,
then this setting controls if extra headers are written to make the
resulting file extra hardened against malfunction.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omfile`.

:Name: veryRobustZip
:Scope: action
:Type: boolean
:Default: action=off
:Required?: no
:Introduced: 7.3.0

Description
-----------

If *zipLevel* is greater than 0,
then this setting controls if extra headers are written to make the
resulting file extra hardened against malfunction. If set to off,
data appended to previously unclean closed files may not be
accessible without extra tools (like `gztool <https://github.com/circulosmeos/gztool>`_ with: ``gztool -p``).
Note that this risk is usually
expected to be bearable, and thus "off" is the default mode. The
extra headers considerably degrade compression, files with this
option set to "on" may be four to five times as large as files
processed in "off" mode.

**In order to avoid this degradation in compression** both
*flushOnTXEnd* and *asyncWriting* parameters must be set to "off"
and also *ioBufferSize* must be raised from default "4k" value to
at least "32k". This way a reasonable compression factor is
maintained, similar to a non-blocked gzip file:

.. code-block:: none

   veryRobustZip="on" ioBufferSize="64k" flushOnTXEnd="off" asyncWriting="off"


Do not forget to add your desired *zipLevel* to this configuration line.

Action usage
------------

.. _param-omfile-action-veryrobustzip:
.. _omfile.parameter.action.veryrobustzip:
.. code-block:: rsyslog

   action(type="omfile" veryRobustZip="...")

Notes
-----

- Legacy documentation referred to the type as ``binary``; this maps to ``boolean``.

See also
--------

See also :doc:`../../configuration/modules/omfile`.
