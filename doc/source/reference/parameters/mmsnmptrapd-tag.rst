.. _param-mmsnmptrapd-tag:
.. _mmsnmptrapd.parameter.module.tag:

Tag
===

.. index::
   single: mmsnmptrapd; Tag
   single: Tag

.. summary-start

Specifies the tag prefix that identifies messages for processing.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/mmsnmptrapd`.

:Name: Tag
:Scope: module
:Type: word (see :doc:`../../rainerscript/constant_strings`)
:Default: module=snmptrapd
:Required?: no
:Introduced: at least 5.8.1, possibly earlier

Description
-----------
Tells the module which start string inside the tag to look for. The default is
``snmptrapd``. Note that a slash is automatically added to this tag when it
comes to matching incoming messages. It MUST not be given, except if two
slashes are required for whatever reasons (so ``tag/`` results in a check for
``tag//`` at the start of the tag field).

Module usage
------------
.. _param-mmsnmptrapd-module-tag:
.. _mmsnmptrapd.parameter.module.tag-usage:

.. code-block:: rsyslog

   module(load="mmsnmptrapd" Tag="snmptrapd")

Legacy names (for reference)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Historic names/directives for compatibility. Do not use in new configs.

.. _mmsnmptrapd.parameter.legacy.mmsnmptrapdtag:

- $mmsnmptrapdTag — maps to Tag (status: legacy)

.. index::
   single: mmsnmptrapd; $mmsnmptrapdTag
   single: $mmsnmptrapdTag

See also
--------
See also :doc:`../../configuration/modules/mmsnmptrapd`.
