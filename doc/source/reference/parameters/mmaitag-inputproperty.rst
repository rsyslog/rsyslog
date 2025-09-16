.. _param-mmaitag-inputproperty:
.. _mmaitag.parameter.action.inputproperty:

inputProperty
=============

.. index::
   single: mmaitag; inputProperty
   single: inputProperty

.. summary-start

Selects which message property is classified instead of the raw message.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/mmaitag`.

:Name: inputProperty
:Scope: action
:Type: string
:Default: none (raw message is used)
:Required?: no
:Introduced: 9.0.0

Description
-----------
Specifies which message property to classify. If unset, the module uses the
raw message text.

Action usage
-------------
.. _param-mmaitag-action-inputproperty:
.. _mmaitag.parameter.action.inputproperty-usage:

.. code-block:: rsyslog

   action(type="mmaitag" inputProperty="msg")

See also
--------
* :doc:`../../configuration/modules/mmaitag`
