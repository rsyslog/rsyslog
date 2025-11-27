.. _param-ommail-server:
.. _ommail.parameter.input.server:

Server
======

.. index::
   single: ommail; Server
   single: Server

.. summary-start

Specifies the hostname or IP address of the SMTP server used by ommail actions.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/ommail`.

:Name: Server
:Scope: input
:Type: word
:Default: input=none
:Required?: yes
:Introduced: 8.5.0

Description
-----------
Name or IP address of the SMTP server to be used. Must currently be set. The default SMTP server is 127.0.0.1 on the local machine, but it should generally be specified explicitly.

Input usage
------------
.. _ommail.parameter.input.server-usage:

.. code-block:: rsyslog

   module(load="ommail")
   action(type="ommail" server="mail.example.net")

Legacy names (for reference)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Historic names/directives for compatibility. Do not use in new configs.

.. _ommail.parameter.legacy.actionmailsmtpserver:

- $ActionMailSMTPServer — maps to Server (status: legacy)

.. index::
   single: ommail; $ActionMailSMTPServer
   single: $ActionMailSMTPServer

See also
--------
See also :doc:`../../configuration/modules/ommail`.
