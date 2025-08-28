.. _param-omfile-file:
.. _omfile.parameter.module.file:

File
====

.. index::
   single: omfile; File
   single: File

.. summary-start

This creates a static file output, always writing into the same file.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omfile`.

:Name: File
:Scope: action
:Type: string
:Default: action=none
:Required?: no
:Introduced: at least 8.x, possibly earlier

Description
-----------

This creates a static file output, always writing into the same file.
If the file already exists, new data is appended to it. Existing
data is not truncated. If the file does not already exist, it is
created. Files are kept open as long as rsyslogd is active. This
conflicts with external log file rotation. In order to close a file
after rotation, send rsyslogd a HUP signal after the file has been
rotated away. Either file or dynaFile can be used, but not both. If both
are given, dynaFile will be used.

Action usage
------------

.. _param-omfile-action-file:
.. _omfile.parameter.action.file:
.. code-block:: rsyslog

   action(type="omfile" File="...")

See also
--------

See also :doc:`../../configuration/modules/omfile`.
