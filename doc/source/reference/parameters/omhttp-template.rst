.. _param-omhttp-template:
.. _omhttp.parameter.module.template:

template
========

.. index::
   single: omhttp; template
   single: template

.. summary-start

Selects the rsyslog template used to render each message before omhttp submits it.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omhttp`.

:Name: template
:Scope: module
:Type: word
:Default: module=StdJSONFmt
:Required?: no
:Introduced: Not specified

Description
-----------
The template to be used for the messages.

Note that in batching mode, this describes the format of *each* individual message, *not* the format of the resulting batch. Some batch modes require that a template produces valid JSON.

Module usage
------------
.. _omhttp.parameter.module.template-usage:

.. code-block:: rsyslog

   template(name="tpl_omhttp_json" type="list") {
      constant(value="{")   property(name="msg"           outname="message"   format="jsonfr")
      constant(value=",")   property(name="hostname"      outname="host"      format="jsonfr")
      constant(value=",")   property(name="timereported"  outname="timestamp" format="jsonfr" dateFormat="rfc3339")
      constant(value="}")
   }

   module(load="omhttp")

   action(
      type="omhttp"
      template="tpl_omhttp_json"
   )

See also
--------
See also :doc:`../../configuration/modules/omhttp`.
