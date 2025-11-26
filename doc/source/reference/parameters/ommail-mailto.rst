.. _param-ommail-mailto:
.. _ommail.parameter.module.mailto:

MailTo
======

.. index::
   single: ommail; MailTo
   single: MailTo

.. summary-start
 
Provides one or more recipient email addresses for each mail sent by ommail.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/ommail`.

:Name: MailTo
:Scope: module
:Type: array
:Default: module=none
:Required?: yes
:Introduced: 8.5.0

Description
-----------
The recipient email address(es). This is an array parameter, so multiple recipients can be provided in a list.

Module usage
------------
.. _param-ommail-module-mailto:
.. _ommail.parameter.module.mailto-usage:

.. code-block:: rsyslog

   module(load="ommail")
   action(type="ommail" mailTo=["operator@example.net", "admin@example.net"])

Legacy names (for reference)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Historic names/directives for compatibility. Do not use in new configs.

.. _ommail.parameter.legacy.actionmailto:

- $ActionMailTo — maps to MailTo (status: legacy)

.. index::
   single: ommail; $ActionMailTo
   single: $ActionMailTo

See also
--------
See also :doc:`../../configuration/modules/ommail`.
