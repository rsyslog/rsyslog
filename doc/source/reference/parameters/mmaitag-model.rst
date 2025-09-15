.. _param-mmaitag-model:
.. _mmaitag.parameter.action.model:

model
=====

.. index::
   single: mmaitag; model
   single: model

.. summary-start

Specifies the AI model identifier used by the provider.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/mmaitag`.

:Name: model
:Scope: action
:Type: string
:Default: gemini-2.0-flash
:Required?: no
:Introduced: 9.0.0

Description
-----------
AI model identifier to use. The meaning of the value is provider specific.

Action usage
-------------
.. _param-mmaitag-action-model:
.. _mmaitag.parameter.action.model-usage:

.. code-block:: rsyslog

   action(type="mmaitag" model="gemini-1.5-flash")

See also
--------
See also :doc:`../../configuration/modules/mmaitag`.
