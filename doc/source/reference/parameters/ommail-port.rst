.. _param-ommail-port:
.. _ommail.parameter.input.port:

port
====

.. index::
   single: ommail; port
   single: port

.. summary-start

Sets the SMTP port number or service name that ommail uses in SMTP mode.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/ommail`.

:Name: port
:Scope: input
:Type: word
:Default: input=none
:Required?: yes
:Introduced: 8.5.0

Description
-----------
Port number or name of the SMTP port to be used by ``mode="smtp"``. While
the module may fall back to port 25 in some legacy scenarios, this parameter
is required for modern SMTP-mode configurations. It is not used by
``mode="sendmail"``.

Input usage
------------
.. _ommail.parameter.input.port-usage:

.. code-block:: rsyslog

   module(load="ommail")
   action(
       type="ommail"
       server="mail.example.net"
       port="25"
       mailFrom="rsyslog@example.net"
       mailTo="operator@example.net"
   )

Legacy names (for reference)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Historic names/directives for compatibility. Do not use in new configs.

.. _ommail.parameter.legacy.actionmailsmtpport:

- $ActionMailSMTPPort — maps to port (status: legacy)

.. index::
   single: ommail; $ActionMailSMTPPort
   single: $ActionMailSMTPPort

See also
--------
See also :doc:`../../configuration/modules/ommail`.
