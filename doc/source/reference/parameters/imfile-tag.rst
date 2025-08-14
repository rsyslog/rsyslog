.. _param-imfile-tag:
.. _imfile.parameter.input.tag:
.. _imfile.parameter.tag:

Tag
===

.. index::
   single: imfile; Tag
   single: Tag

.. summary-start

Assigns a tag to messages read from the file; include a colon if desired.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imfile`.

:Name: Tag
:Scope: input
:Type: string (see :doc:`../../rainerscript/constant_strings`)
:Default: none
:Required?: yes
:Introduced: at least 5.x, possibly earlier

Description
-----------
The tag to be assigned to messages read from this file. If a colon should
follow the tag, include it in the value, for example
``Tag="myTag:"``.

Input usage
-----------
.. _param-imfile-input-tag:
.. _imfile.parameter.input.tag-usage:

.. code-block:: rsyslog

   input(type="imfile" Tag="app:")

Legacy names (for reference)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Historic names/directives for compatibility. Do not use in new configs.

.. _imfile.parameter.legacy.inputfiletag:
- ``$InputFileTag`` — maps to Tag (status: legacy)

.. index::
   single: imfile; $InputFileTag
   single: $InputFileTag

See also
--------
See also :doc:`../../configuration/modules/imfile`.
