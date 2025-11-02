.. _param-omhttp-statsname:
.. _omhttp.parameter.input.statsname:

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
:Scope: input
:Type: word
:Default: input=none
:Required?: no
:Introduced: Not specified

Description
-----------
The name assigned to statistics specific to this action instance. The supported set of statistics tracked for this action instance are **submitted**, **acked**, **failures**. See the `Statistic Counter` section of :doc:`../../configuration/modules/omhttp` for more details.

Input usage
-----------
.. _omhttp.parameter.input.statsname-usage:

.. code-block:: rsyslog

   module(load="omhttp")

   action(
       type="omhttp"
       statsName="omhttp_api"
   )

See also
--------
See also :doc:`../../configuration/modules/omhttp`.
