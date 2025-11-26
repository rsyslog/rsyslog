.. _param-ommail-template:
.. _ommail.parameter.module.template:

Template
========

.. index::
   single: ommail; Template
   single: Template

.. summary-start

Selects the template used for the mail body when body text is enabled.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/ommail`.

:Name: Template
:Scope: module
:Type: word
:Default: module=RSYSLOG_FileFormat
:Required?: no
:Introduced: 8.5.0

Description
-----------
Template to be used for the mail body when body output is enabled.

Module usage
------------
.. _param-ommail-module-template:
.. _ommail.parameter.module.template-usage:

.. code-block:: rsyslog

   module(load="ommail")
   action(type="ommail" template="mailBody")

See also
--------
See also :doc:`../../configuration/modules/ommail`.
