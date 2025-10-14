.. _param-impstats-facility:
.. _impstats.parameter.module.facility:

facility
========

.. index::
   single: impstats; facility
   single: facility

.. summary-start

Selects the numeric syslog facility value used for generated statistics messages.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/impstats`.

:Name: facility
:Scope: module
:Type: integer
:Default: module=5
:Required?: no
:Introduced: at least 5.x, possibly earlier

Description
-----------
The numerical syslog facility code to be used for generated messages. Default
is 5 (syslog). This is useful for filtering messages.

Module usage
------------
.. _impstats.parameter.module.facility-usage:

.. code-block:: rsyslog

   module(load="impstats" facility="16")

See also
--------
See also :doc:`../../configuration/modules/impstats`.
