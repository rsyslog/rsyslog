.. _param-ommail-sendmail-binary:
.. _ommail.parameter.input.sendmail-binary:

sendmail.binary
===============

.. index::
   single: ommail; sendmail.binary
   single: sendmail.binary

.. summary-start

Specifies the sendmail-compatible binary used when ommail runs in
sendmail mode.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/ommail`.

:Name: sendmail.binary
:Scope: input
:Type: string
:Default: input=/usr/sbin/sendmail
:Required?: no
:Introduced: 8.2606.0

Description
-----------
Configures the binary executed for ``mode="sendmail"``. The default
``/usr/sbin/sendmail`` follows the standard local mail submission
interface. Ommail verifies that the configured binary is executable before
attempting delivery.

Ommail invokes the binary without a shell and passes recipients as
separate arguments.

Input usage
-----------
.. _ommail.parameter.input.sendmail-binary-usage:

.. code-block:: rsyslog

   module(load="ommail")
   action(
       type="ommail"
       mode="sendmail"
       sendmail.binary="/usr/sbin/sendmail"
       mailFrom="rsyslog@example.net"
       mailTo="operator@example.net"
   )

See also
--------
See also :doc:`../../configuration/modules/ommail`.
