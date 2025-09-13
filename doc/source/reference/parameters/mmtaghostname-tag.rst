.. _param-mmtaghostname-tag:
.. _mmtaghostname.parameter.input.tag:

tag
===

.. index::
   single: mmtaghostname; tag
   single: tag

.. summary-start

Assigns a tag to messages. This is useful for inputs like ``imudp`` or
``imtcp`` that do not provide a tag.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/mmtaghostname`.

:Name: tag
:Scope: input
:Type: string
:Default: none
:Required?: no
:Introduced: at least 7.0, possibly earlier

Description
-----------
The tag to be assigned to messages processed by this module. This is often
used to route messages to different rulesets or output actions based on the
tag. If you would like to see the colon after the tag, you need to include it
when you assign a tag value, like so: ``tag="myTagValue:"``. If this
parameter is not set, message tags are not modified.

Input usage
-----------
.. _mmtaghostname.parameter.input.tag-usage:

.. code-block:: rsyslog

   action(type="mmtaghostname"
          tag="front")

See also
--------
* :doc:`../../configuration/modules/mmtaghostname`
