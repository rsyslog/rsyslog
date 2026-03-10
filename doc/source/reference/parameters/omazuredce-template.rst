.. _param-omazuredce-template:
.. _omazuredce.parameter.action.template:

.. meta::
   :description: Reference for the omazuredce template parameter.
   :keywords: rsyslog, omazuredce, template, azure

template
========

.. index::
   single: omazuredce; template
   single: template

.. summary-start

Selects the rsyslog template used to render each message before it is added to
the Azure ingestion batch.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omazuredce`.

:Name: template
:Scope: action
:Type: word
:Default: action=StdJSONFmt
:Required?: no
:Introduced: Not specified

Description
-----------
The selected template must render exactly one valid JSON object per message.
``omazuredce`` parses the rendered text and merges it into the batch payload.
If this parameter is omitted, the module uses the built-in ``StdJSONFmt``
template, which already renders one JSON object per message.

Action usage
------------
.. _omazuredce.parameter.action.template-usage:

.. code-block:: rsyslog

   template(name="tplAzureDce" type="list" option.jsonf="on") {
      property(outname="TimeGenerated" name="timereported" dateFormat="rfc3339" format="jsonf")
      property(outname="Host" name="hostname" format="jsonf")
      property(outname="Message" name="msg" format="jsonf")
   }

   action(type="omazuredce" template="tplAzureDce" ...)

See also
--------
See also :doc:`../../configuration/modules/omazuredce`.
