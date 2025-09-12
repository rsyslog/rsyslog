.. _param-mmtaghostname-tag:
.. _mmtaghostname.parameter.input.tag:

Tag
===

.. index::
   single: mmtaghostname; Tag
   single: Tag

.. summary-start

Assigns a tag to messages processed by mmtaghostname.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/mmtaghostname`.

:Name: Tag
:Scope: input
:Type: string
:Default: none
:Required?: no
:Introduced: at least 7.0, possibly earlier

Description
-----------
The tag to be assigned to messages modified. If you would like to see the
colon after the tag, you need to include it when you assign a tag value,
like so: ``tag="myTagValue:"``.
If this attribute is not provided, message tags are not modified.

Input usage
-----------
.. _mmtaghostname.parameter.input.tag-usage:

.. code-block:: rsyslog

   action(type="mmtaghostname"
          tag="front")

See also
--------
See also :doc:`../../configuration/modules/mmtaghostname`.
