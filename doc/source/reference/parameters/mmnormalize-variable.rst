.. _param-mmnormalize-variable:
.. _mmnormalize.parameter.input.variable:

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
:Scope: input
:Type: word
:Default: input=msg
:Required?: no
:Introduced: 8.5.1

Description
-----------
.. versionadded:: 8.5.1

Specifies if a variable instead of property 'msg' should be used for normalization. A variable can be property, local variable, json-path etc. Please note that **useRawMsg** overrides this parameter, so if **useRawMsg** is set, **variable** will be ignored and raw message will be used.

Input usage
-----------
.. _param-mmnormalize-input-variable:
.. _mmnormalize.parameter.input.variable-usage:

.. code-block:: rsyslog

   action(type="mmnormalize" variable="$.foo")

See also
--------
See also :doc:`../../configuration/modules/mmnormalize`.
