.. _param-omhttp-statsname:
.. _omhttp.parameter.module.statsname:

statsname
=========

.. index::
   single: omhttp; statsname
   single: statsname

.. summary-start

Assigns a dedicated statistics counter origin name for this omhttp action.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omhttp`.

:Name: statsname
:Scope: module
:Type: word
:Default: module=none
:Required?: no
:Introduced: Not specified

Description
-----------
The name assigned to statistics specific to this action instance. The supported set of statistics tracked for this action instance are **submitted**, **acked**, **failures**. See the `Statistic Counter` section of :doc:`../../configuration/modules/omhttp` for more details.

Module usage
------------
.. _param-omhttp-module-statsname:
.. _omhttp.parameter.module.statsname-usage:

.. code-block:: rsyslog

   module(load="omhttp")

   action(
       type="omhttp"
       statsName="omhttp_api"
   )

See also
--------
See also :doc:`../../configuration/modules/omhttp`.
