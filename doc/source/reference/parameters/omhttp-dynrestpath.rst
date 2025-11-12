.. _param-omhttp-dynrestpath:
.. _omhttp.parameter.input.dynrestpath:

dynrestpath
===========

.. index::
   single: omhttp; dynrestpath
   single: dynrestpath

.. summary-start

Enables using a template name in :ref:`param-omhttp-restpath` so each message can select a REST path dynamically.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omhttp`.

:Name: dynrestpath
:Scope: input
:Type: boolean
:Default: input=off
:Required?: no
:Introduced: Not specified

Description
-----------
When this parameter is set to ``on`` you can specify a template name in the :ref:`param-omhttp-restpath` parameter instead of the actual path. This way you can use dynamic REST paths for your messages based on the template you are using.

Input usage
-----------
.. _omhttp.parameter.input.dynrestpath-usage:

.. code-block:: rsyslog

   template(name="tpl_dynamic_path" type="string" string="tenant/%hostname%/events")

   module(load="omhttp")

   action(
       type="omhttp"
       dynRestPath="on"
       restPath="tpl_dynamic_path"
   )

See also
--------
See also :doc:`../../configuration/modules/omhttp`.
