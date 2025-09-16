.. _param-mmaitag-provider:
.. _mmaitag.parameter.action.provider:

provider
========

.. index::
   single: mmaitag; provider
   single: provider

.. summary-start

Selects which backend provider processes the classification (``gemini`` or ``gemini_mock``).

.. summary-end

This parameter applies to :doc:`../../configuration/modules/mmaitag`.

:Name: provider
:Scope: action
:Type: string
:Default: gemini
:Required?: no
:Introduced: 9.0.0

Description
-----------
Specifies which backend to use for classification. Supported values are
``gemini`` and ``gemini_mock``.

Action usage
-------------
.. _param-mmaitag-action-provider:
.. _mmaitag.parameter.action.provider-usage:

.. code-block:: rsyslog

   action(type="mmaitag" provider="gemini_mock")

See also
--------
* :doc:`../../configuration/modules/mmaitag`
