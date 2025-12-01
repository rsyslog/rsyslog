.. _param-ommail-mailfrom:
.. _ommail.parameter.input.mailfrom:

mailFrom
========

.. index::
   single: ommail; mailFrom
   single: mailFrom

.. summary-start

Defines the sender email address placed in outgoing messages from ommail.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/ommail`.

:Name: mailFrom
:Scope: input
:Type: word
:Default: input=none
:Required?: yes
:Introduced: 8.5.0

Description
-----------
This mandatory parameter defines the email address used as the sender. It will
appear in the ``From:`` header of the email.

Input usage
------------
.. _ommail.parameter.input.mailfrom-usage:

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

.. _ommail.parameter.legacy.actionmailfrom:

- $ActionMailFrom — maps to mailFrom (status: legacy)

.. index::
   single: ommail; $ActionMailFrom
   single: $ActionMailFrom

See also
--------
See also :doc:`../../configuration/modules/ommail`.
