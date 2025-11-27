.. _param-ommail-port:
.. _ommail.parameter.input.port:

Port
====

.. index::
   single: ommail; Port
   single: Port

.. summary-start

Sets the SMTP port number or service name that ommail uses to send mail.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/ommail`.

:Name: Port
:Scope: input
:Type: word
:Default: input=none
:Required?: yes
:Introduced: 8.5.0

Description
-----------
Port number or name of the SMTP port to be used. While the module may fall
back to port 25 in some legacy scenarios, this parameter is required and must
be set explicitly in modern configurations.

Input usage
------------
.. _ommail.parameter.input.port-usage:

.. code-block:: rsyslog

   module(load="ommail")
   action(type="ommail" port="25")

Legacy names (for reference)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Historic names/directives for compatibility. Do not use in new configs.

.. _ommail.parameter.legacy.actionmailsmtpport:

- $ActionMailSMTPPort — maps to Port (status: legacy)

.. index::
   single: ommail; $ActionMailSMTPPort
   single: $ActionMailSMTPPort

See also
--------
See also :doc:`../../configuration/modules/ommail`.
