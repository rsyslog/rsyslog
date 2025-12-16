.. _param-omhttp-healthchecktimedelay:
.. _omhttp.parameter.input.healthchecktimedelay:

healthchecktimedelay
====================

.. index::
   single: omhttp; healthchecktimedelay
   single: healthchecktimedelay

.. summary-start

Sets the number of seconds omhttp waits before doing an other health check to the API.
This parameter is used only if "checkpath" is set up.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omhttp`.

:Name: healthchecktimedelay
:Scope: input
:Type: integer
:Default: input=-1
:Required?: no
:Introduced: Not specified

Description
-----------
Sets the number of seconds omhttp waits before doing another health check to the API.
This parameter is used only if "checkpath" is set up.

Input usage
-----------
.. _omhttp.parameter.input.healthchecktimedelay-usage:

.. code-block:: rsyslog

   module(load="omhttp")

   action(
       type="omhttp"
       healthchecktimedelay="60"
   )

See also
--------
See also :doc:`../../configuration/modules/omhttp`.
