.. _param-omhttp-reloadonhup:
.. _omhttp.parameter.module.reloadonhup:

reloadonhup
===========

.. index::
   single: omhttp; reloadonhup
   single: reloadonhup

.. summary-start

Reinitializes libcurl handles on ``HUP`` signals so certificates can be refreshed without restarting rsyslog.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omhttp`.

:Name: reloadonhup
:Scope: module
:Type: boolean
:Default: module=off
:Required?: no
:Introduced: Not specified

Description
-----------
If this parameter is ``on``, the plugin will close and reopen any libcurl handles on a HUP signal. This option is primarily intended to enable reloading short-lived certificates without restarting rsyslog.

Module usage
------------
.. _param-omhttp-module-reloadonhup:
.. _omhttp.parameter.module.reloadonhup-usage:

.. code-block:: rsyslog

   module(load="omhttp")

   action(
       type="omhttp"
       reloadOnHup="on"
   )

See also
--------
See also :doc:`../../configuration/modules/omhttp`.
