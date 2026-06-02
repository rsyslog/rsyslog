.. _param-ommail-mode:
.. _ommail.parameter.input.mode:

mode
====

.. index::
   single: ommail; mode
   single: mode

.. summary-start

Selects whether ommail sends mail directly via SMTP or through a
sendmail-compatible local submission program.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/ommail`.

:Name: mode
:Scope: input
:Type: word
:Default: input=smtp
:Required?: no
:Introduced: 8.2606.0

Description
-----------
Use ``smtp`` to let ommail talk directly to an SMTP server. Use
``sendmail`` to invoke a sendmail-compatible binary and write the mail
message to its standard input.

Input usage
-----------
.. _ommail.parameter.input.mode-usage:

.. code-block:: rsyslog

   module(load="ommail")
   action(
       type="ommail"
       mode="sendmail"
       mailFrom="rsyslog@example.net"
       mailTo="operator@example.net"
   )

See also
--------
See also :doc:`../../configuration/modules/ommail`.
