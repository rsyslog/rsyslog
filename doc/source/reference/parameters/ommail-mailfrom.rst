.. _param-ommail-mailfrom:
.. _ommail.parameter.input.mailfrom:

MailFrom
========

.. index::
   single: ommail; MailFrom
   single: MailFrom

.. summary-start

Defines the sender email address placed in outgoing messages from ommail.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/ommail`.

:Name: MailFrom
:Scope: input
:Type: word
:Default: input=none
:Required?: yes
:Introduced: 8.5.0

Description
-----------
The email address used as the sender's address.

Input usage
------------
.. _ommail.parameter.input.mailfrom-usage:

.. code-block:: rsyslog

   module(load="ommail")
   action(type="ommail" mailFrom="rsyslog@example.net")

Legacy names (for reference)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Historic names/directives for compatibility. Do not use in new configs.

.. _ommail.parameter.legacy.actionmailfrom:

- $ActionMailFrom — maps to MailFrom (status: legacy)

.. index::
   single: ommail; $ActionMailFrom
   single: $ActionMailFrom

See also
--------
See also :doc:`../../configuration/modules/ommail`.
