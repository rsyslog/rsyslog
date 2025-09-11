.. _param-mmsnmptrapd-tag:
.. _mmsnmptrapd.parameter.module.tag:

tag
===

.. index::
   single: mmsnmptrapd; tag
   single: tag

.. summary-start

Specifies the tag prefix that identifies messages for processing.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/mmsnmptrapd`.

:Name: tag
:Scope: module
:Type: word (see :doc:`../../rainerscript/constant_strings`)
:Default: module=snmptrapd
:Required?: no
:Introduced: at least 5.8.1, possibly earlier

Description
-----------
Tells the module which start string inside the tag to look for. The default is
``snmptrapd``. Note that a slash (``/``) is automatically appended to this tag for
matching. You should not include a trailing slash unless you specifically need
to match a double slash. For example, setting ``tag="tag/"`` results in a
check for ``tag//`` at the start of the tag field.

Module usage
------------
.. _param-mmsnmptrapd-module-tag:
.. _mmsnmptrapd.parameter.module.tag-usage:

.. code-block:: rsyslog

   module(load="mmsnmptrapd" tag="snmptrapd")

Legacy names (for reference)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Historic names/directives for compatibility. Do not use in new configs.

.. _mmsnmptrapd.parameter.legacy.mmsnmptrapdtag:

- $mmsnmptrapdTag — maps to tag (status: legacy)

.. index::
   single: mmsnmptrapd; $mmsnmptrapdTag
   single: $mmsnmptrapdTag

See also
--------
See also :doc:`../../configuration/modules/mmsnmptrapd`.
