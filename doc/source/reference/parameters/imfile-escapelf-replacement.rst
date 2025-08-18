.. _param-imfile-escapelf-replacement:
.. _imfile.parameter.input.escapelf-replacement:
.. _imfile.parameter.escapelf-replacement:

escapeLF.replacement
====================

.. index::
   single: imfile; escapeLF.replacement
   single: escapeLF.replacement

.. summary-start

Overrides the default LF escape sequence when ``escapeLF`` is ``on``.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imfile`.

:Name: escapeLF.replacement
:Scope: input
:Type: string (see :doc:`../../rainerscript/constant_strings`)
:Default: depending on use
:Required?: no
:Introduced: 8.2001.0

Description
-----------
Works together with ``escapeLF``. It is honored only if
``escapeLF="on"``. It permits replacing the default escape sequence,
which historically may be ``#012`` or ``\n``. If not configured, that default
remains.

Examples:

.. code-block:: rsyslog

   escapeLF.replacement=" "
   escapeLF.replacement="[LF]"
   escapeLF.replacement=""

Input usage
-----------
.. _param-imfile-input-escapelf-replacement:
.. _imfile.parameter.input.escapelf-replacement-usage:

.. code-block:: rsyslog

   input(type="imfile"
         File="/var/log/example.log"
         Tag="example"
         escapeLF="on" escapeLF.replacement="[LF]")

See also
--------
See also :doc:`../../configuration/modules/imfile`.
