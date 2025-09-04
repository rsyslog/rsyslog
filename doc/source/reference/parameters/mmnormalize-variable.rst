.. _param-mmnormalize-variable:
.. _mmnormalize.parameter.action.variable:

variable
========

.. index::
   single: mmnormalize; variable
   single: variable

.. summary-start

Normalizes the content of a specified variable instead of the default msg property.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/mmnormalize`.

:Name: variable
:Scope: action
:Type: word
:Default: action=msg
:Required?: no
:Introduced: 8.5.1

Description
-----------
.. versionadded:: 8.5.1

Specifies if a variable instead of property 'msg' should be used for normalization. A variable can be property, local variable, json-path etc. Please note that **useRawMsg** overrides this parameter, so if **useRawMsg** is set, **variable** will be ignored and raw message will be used.

Action usage
-------------
.. _param-mmnormalize-action-variable:
.. _mmnormalize.parameter.action.variable-usage:

.. code-block:: rsyslog

   action(type="mmnormalize" variable="$.foo")

See also
--------
See also :doc:`../../configuration/modules/mmnormalize`.
