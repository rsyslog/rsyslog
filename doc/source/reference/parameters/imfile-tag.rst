.. _param-imfile-tag:
.. _imfile.parameter.module.tag:

Tag
===

.. index::
   single: imfile; Tag
   single: Tag

.. summary-start

The tag to be assigned to messages read from this file.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imfile`.

:Name: Tag
:Scope: input
:Type: string (see :doc:`../../rainerscript/constant_strings`)
:Default: input=none
:Required?: yes
:Introduced: at least 5.x, possibly earlier

Description
-----------
The tag to be assigned to messages read from this file. If you would like to
see the colon after the tag, you need to include it when you assign a tag
value, like so: ``tag="myTagValue:"``.

Input usage
-----------
.. _param-imfile-input-tag:
.. _imfile.parameter.input.tag:
.. code-block:: rsyslog

   input(type="imfile" Tag="...")

Legacy names (for reference)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Historic names/directives for compatibility. Do not use in new configs.

.. _imfile.parameter.legacy.inputfiletag:
- $InputFileTag — maps to Tag (status: legacy)

.. index::
   single: imfile; $InputFileTag
   single: $InputFileTag

See also
--------
See also :doc:`../../configuration/modules/imfile`.
