.. _param-ommail-server:
.. _ommail.parameter.input.server:

server
======

.. index::
   single: ommail; server
   single: server

.. summary-start

Specifies the hostname or IP address of the SMTP server used by ommail actions.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/ommail`.

:Name: server
:Scope: input
:Type: word
:Default: input=none
:Required?: yes
:Introduced: 8.5.0

Description
-----------
Name or IP address of the SMTP server to be used. While a default of
127.0.0.1 (the local machine) exists for legacy configurations, this
parameter is required and must be set explicitly in modern configurations.

Input usage
------------
.. _ommail.parameter.input.server-usage:

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

.. _ommail.parameter.legacy.actionmailsmtpserver:

- $ActionMailSMTPServer — maps to server (status: legacy)

.. index::
   single: ommail; $ActionMailSMTPServer
   single: $ActionMailSMTPServer

See also
--------
See also :doc:`../../configuration/modules/ommail`.
