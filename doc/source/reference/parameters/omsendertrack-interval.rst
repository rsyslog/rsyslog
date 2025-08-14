.. _param-omsendertrack-interval:
.. _omsendertrack.parameter.module.interval:

interval
========

.. index::
   single: omsendertrack; interval
   single: interval

.. summary-start

Seconds between persistence of sender statistics to the state file.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omsendertrack`.

:Name: interval
:Scope: action
:Type: integer
:Default: action=60
:Required?: no
:Introduced: 8.2506.0

Description
-----------
Sets how often, in seconds, the module writes accumulated sender data to disk.

Action usage
------------
.. _param-omsendertrack-action-interval:
.. _omsendertrack.parameter.action.interval:

.. code-block:: rsyslog

   action(type="omsendertrack" interval="60")

See also
--------
See also :doc:`../../configuration/modules/omsendertrack`.

