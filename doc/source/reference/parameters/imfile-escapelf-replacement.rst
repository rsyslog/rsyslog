.. _param-imfile-escapelf-replacement:
.. _imfile.parameter.module.escapelf-replacement:

escapeLF.replacement
====================

.. index::
   single: imfile; escapeLF.replacement
   single: escapeLF.replacement

.. summary-start

.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imfile`.

:Name: escapeLF.replacement
:Scope: input
:Type: string (see :doc:`../../rainerscript/constant_strings`)
:Default: input=depending on use
:Required?: no
:Introduced: 8.2001.0

Description
-----------
.. versionadded:: 8.2001.0

This parameter works in conjunction with `escapeLF`. It is only
honored if `escapeLF="on"`.

It permits to replace the default escape sequence by a different character
sequence. The default historically is inconsistent and depends on which
functionality is used to read the file. It can be either "#012" or "\\n". If
you want to retain that default, do not configure this parameter.

If it is configured, any sequence may be used. For example, to replace an LF
with a simple space, use::

   escapeLF.replacement=" "

It is also possible to configure longer replacements. An example for this is::

   escapeLF.replacement="[LF]"

Finally, it is possible to completely remove the LF. This is done by specifying
an empty replacement sequence::

   escapeLF.replacement=""

Input usage
-----------
.. _param-imfile-input-escapelf-replacement:
.. _imfile.parameter.input.escapelf-replacement:
.. code-block:: rsyslog

   input(type="imfile" escapeLF.replacement="...")

See also
--------
See also :doc:`../../configuration/modules/imfile`.
