.. _param-omhttp-usehttps:
.. _omhttp.parameter.input.usehttps:

useHttps
========

.. index::
   single: omhttp; useHttps
   single: useHttps

.. summary-start

Switches omhttp to use HTTPS instead of HTTP when sending requests.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omhttp`.

:Name: useHttps
:Scope: input
:Type: boolean
:Default: input=off
:Required?: no
:Introduced: Not specified

Description
-----------
When switched to ``on`` you will use ``https`` instead of ``http``.

Input usage
-----------
.. _omhttp.parameter.input.usehttps-usage:

.. code-block:: rsyslog

   module(load="omhttp")

   action(
       type="omhttp"
       useHttps="on"
   )

See also
--------
See also :doc:`../../configuration/modules/omhttp`.
