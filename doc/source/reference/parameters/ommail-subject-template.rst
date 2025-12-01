.. _param-ommail-subject-template:
.. _ommail.parameter.input.subject-template:

subjectTemplate
================

.. index::
   single: ommail; subjectTemplate
   single: subjectTemplate

.. summary-start

Selects the template used to generate the mail subject line.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/ommail`.

:Name: subjectTemplate
:Scope: input
:Type: word
:Default: input=none
:Required?: no
:Introduced: 8.5.0

Description
-----------
The name of the template to be used as the mail subject. Use this parameter
when message content should appear in the subject. If you just need a constant
subject line, use :ref:`param-ommail-subject-text` instead. The
``subjectTemplate`` and ``subjectText`` parameters cannot both be configured
within a single action. If neither is specified, a default, non-configurable
subject line will be generated.

Input usage
------------
.. _ommail.parameter.input.subject-template-usage:

.. code-block:: rsyslog

   module(load="ommail")
   template(name="mailSubject" type="string" string="Alert: %msg%")
   action(
       type="ommail"
       server="mail.example.net"
       port="25"
       mailFrom="rsyslog@example.net"
       mailTo="operator@example.net"
       subjectTemplate="mailSubject"
   )

Legacy names (for reference)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Historic names/directives for compatibility. Do not use in new configs.

.. _ommail.parameter.legacy.actionmailsubject:

- $ActionMailSubject — maps to subjectTemplate (status: legacy)

.. index::
   single: ommail; $ActionMailSubject
   single: $ActionMailSubject

See also
--------
See also :doc:`../../configuration/modules/ommail`.
