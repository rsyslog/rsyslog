.. _param-mmaitag-tag:
.. _mmaitag.parameter.action.tag:

tag
===

.. index::
   single: mmaitag; tag
   single: tag

.. summary-start

Names the message variable used to store the classification tag.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/mmaitag`.

:Name: tag
:Scope: action
:Type: string
:Default: .aitag
:Required?: no
:Introduced: 9.0.0

Description
-----------
Message variable name for the classification. If the name starts with ``$``,
the leading ``$`` is stripped.

Action usage
-------------
.. _param-mmaitag-action-tag:
.. _mmaitag.parameter.action.tag-usage:

.. code-block:: rsyslog

   action(type="mmaitag" tag="$.aitag")

See also
--------
See also :doc:`../../configuration/modules/mmaitag`.
