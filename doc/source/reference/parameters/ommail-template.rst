.. _param-ommail-template:
.. _ommail.parameter.input.template:

template
========

.. index::
   single: ommail; template
   single: template

.. summary-start

Selects the template used for the mail body when body text is enabled.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/ommail`.

:Name: template
:Scope: input
:Type: word
:Default: input=RSYSLOG_FileFormat
:Required?: no
:Introduced: 8.5.0

Description
-----------
Specifies the name of the template for the mail body. This parameter is only
effective if :ref:`param-ommail-body-enable` is "on" (which is the default).

Input usage
------------
.. _ommail.parameter.input.template-usage:

.. code-block:: rsyslog

   module(load="ommail")
   template(name="mailBody" type="string" string="Message: %msg%")
   action(
       type="ommail"
       server="mail.example.net"
       port="25"
       mailFrom="rsyslog@example.net"
       mailTo="operator@example.net"
       template="mailBody"
   )

See also
--------
See also :doc:`../../configuration/modules/ommail`.
