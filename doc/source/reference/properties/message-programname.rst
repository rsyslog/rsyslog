.. _prop-message-programname:
.. _properties.message.programname:

programname
===========

.. index::
   single: properties; programname
   single: programname

.. summary-start

Extracts the static program name portion from the syslog tag.

.. summary-end

This property belongs to the **Message Properties** group.

:Name: programname
:Category: Message Properties
:Type: string

Description
-----------
The "static" part of the tag, as defined by BSD syslogd. For example, when TAG
is "named[12345]", programname is "named".

Precisely, the programname is terminated by either (whichever occurs first):

- end of tag
- nonprintable character
- ':'
- '['
- '/'

The above definition has been taken from the FreeBSD syslogd sources.

Please note that some applications include slashes in the static part of the
tag, e.g. "app/foo[1234]". In this case, programname is "app". If they store an
absolute path name like in "/app/foo[1234]", programname will become empty
(""). If you need to actually store slashes as part of the programname, you can
use the global option

``global(parser.permitSlashInProgramName="on")``

to permit this. Then, a syslogtag of "/app/foo[1234]" will result in programname
being "/app/foo". Note: this option is available starting at rsyslogd version
8.25.0.

Usage
-----
.. _properties.message.programname-usage:

.. code-block:: rsyslog

   template(name="example" type="string" string="%programname%")

Notes
~~~~~
Use ``global(parser.permitSlashInProgramName="on")`` to allow slashes within the
programname as described above.

See also
--------
See :doc:`../../rainerscript/properties` for the category overview.
