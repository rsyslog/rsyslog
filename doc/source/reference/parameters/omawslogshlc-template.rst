.. _param-omawslogshlc-template:
.. _omawslogshlc.parameter.action.template:

.. meta::
   :description: Reference for the omawslogshlc template parameter.
   :keywords: rsyslog, omawslogshlc, template, aws, cloudwatch

template
========

.. index::
   single: omawslogshlc; template
   single: template

.. summary-start

Specifies the rsyslog template used to render the message content.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omawslogshlc`.

:Name: template
:Scope: action
:Type: word
:Default: RSYSLOG_TraditionalFileFormat
:Required?: no
:Introduced: Not specified

Description
-----------
``template`` controls how each syslog message is rendered before being
placed into the HLC JSON event's ``event`` field. The default
``RSYSLOG_TraditionalFileFormat`` produces the traditional syslog one-line
format.

Action usage
------------
.. _omawslogshlc.parameter.action.template-usage:

.. code-block:: rsyslog

   action(type="omawslogshlc" template="MyCustomTemplate" ...)

See also
--------
See also :doc:`../../configuration/modules/omawslogshlc`.
