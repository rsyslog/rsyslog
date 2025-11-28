.. _param-ommail-body-enable:
.. _ommail.parameter.input.body-enable:

Body.Enable
===========

.. index::
   single: ommail; Body.Enable
   single: Body.Enable

.. summary-start

Toggles inclusion of the mail body in messages sent by ommail.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/ommail`.

:Name: Body.Enable
:Scope: input
:Type: boolean
:Default: input=on
:Required?: no
:Introduced: 8.5.0

Description
-----------
Setting this to "off" permits the actual message body to be excluded. This may
be useful for pager-like devices or cell phone SMS messages. The default is
"on", which is appropriate for almost all cases. Turn it off only if you know
exactly what you do!

Input usage
------------
.. _ommail.parameter.input.body-enable-usage:

.. code-block:: rsyslog

   module(load="ommail")
   action(
       type="ommail"
       server="mail.example.net"
       port="25"
       mailFrom="rsyslog@example.net"
       mailTo="operator@example.net"
       bodyEnable="off"
   )

Legacy names (for reference)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Historic names/directives for compatibility. Do not use in new configs.

.. _ommail.parameter.legacy.actionmailenablebody:

- $ActionMailEnableBody — maps to Body.Enable (status: legacy)

.. index::
   single: ommail; $ActionMailEnableBody
   single: $ActionMailEnableBody

See also
--------
See also :doc:`../../configuration/modules/ommail`.
