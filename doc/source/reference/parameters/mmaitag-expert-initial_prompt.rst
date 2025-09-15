.. _param-mmaitag-expert-initial_prompt:
.. _mmaitag.parameter.action.expert-initial_prompt:

expert.initial_prompt
======================

.. index::
   single: mmaitag; expert.initial_prompt
   single: expert.initial_prompt

.. summary-start

Provides a custom prompt text used by the AI provider before classifying messages.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/mmaitag`.

:Name: expert.initial_prompt
:Scope: action
:Type: string
:Default:
   You are a tool that classifies log messages. Given an array of log
   messages, respond with a JSON array of labels: 'NOISE', 'REGULAR',
   'IMPORTANT', or 'CRITICAL' — one per message, in order. Do not
   include explanations or additional formatting.
:Required?: no
:Introduced: 9.0.0

Description
-----------
Optional custom prompt text. If unset, the following default is used::

    You are a tool that classifies log messages. Given an array of log
    messages, respond with a JSON array of labels: 'NOISE', 'REGULAR',
    'IMPORTANT', or 'CRITICAL' — one per message, in order. Do not
    include explanations or additional formatting.

Action usage
-------------
.. _param-mmaitag-action-expert-initial_prompt:
.. _mmaitag.parameter.action.expert-initial_prompt-usage:

.. code-block:: rsyslog

   action(type="mmaitag" expert.initialPrompt="Classify precisely")

See also
--------
See also :doc:`../../configuration/modules/mmaitag`.
