.. _param-omhttp-restpath:
.. _omhttp.parameter.module.restpath:

restpath
========

.. index::
   single: omhttp; restpath
   single: restpath

.. summary-start

Sets the REST path portion of the request URL used by omhttp.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omhttp`.

:Name: restpath
:Scope: module
:Type: word
:Default: module=none
:Required?: no
:Introduced: Not specified

Description
-----------
The REST path you want to use. Do not include the leading slash character. If the full path looks like ``localhost:5000/my/path``, ``restpath`` should be ``my/path``.

Module usage
------------
.. _omhttp.parameter.module.restpath-usage:

.. code-block:: rsyslog

   module(load="omhttp")

   action(
       type="omhttp"
       restPath="logs/v1/events"
   )

See also
--------
See also :doc:`../../configuration/modules/omhttp`.
