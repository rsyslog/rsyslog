.. _param-mmaitag-expert-initialprompt:
.. _mmaitag.parameter.action.expert-initialprompt:

expert.initialPrompt
=====================

.. index::
   single: mmaitag; expert.initialPrompt
   single: expert.initialPrompt

.. summary-start

Provides a custom prompt text used by the AI provider before classifying messages.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/mmaitag`.

:Name: expert.initialPrompt
:Scope: action
:Type: string
:Default:
   Task: Classify the log message that follows. Output: Exactly one label
   from this list: NOISE, REGULAR, IMPORTANT, CRITICAL. Restrictions: No
   other text, explanations, formatting, or newline characters.
:Required?: no
:Introduced: 9.0.0

Description
-----------
Optional custom prompt text. If unset, the default value is used.

Action usage
-------------
.. _param-mmaitag-action-expert-initialprompt:
.. _mmaitag.parameter.action.expert-initialprompt-usage:

.. code-block:: rsyslog

   action(type="mmaitag" expert.initialPrompt="Classify precisely")

See also
--------
See also :doc:`../../configuration/modules/mmaitag`.
