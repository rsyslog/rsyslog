.. _param-mmdblookup-reloadonhup:
.. _mmdblookup.parameter.input.reloadonhup:

reloadOnHup
===========

.. index::
   single: mmdblookup; reloadOnHup
   single: reloadOnHup

.. summary-start

Controls whether mmdblookup reopens the MaxMind database after a HUP signal.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/mmdblookup`.

:Name: reloadOnHup
:Scope: input
:Type: boolean
:Default: on
:Required?: no
:Introduced: 8.24.0

Description
-----------
When this setting is ``on``, the action closes and reopens the configured
MaxMind DB file whenever rsyslog receives a HUP signal. This lets updated
GeoIP data become effective without restarting the daemon. Set it to ``off``
if you prefer to keep the currently loaded database until the action is
restarted manually.

Input usage
-----------
.. _mmdblookup.parameter.input.reloadonhup-usage:

.. code-block:: rsyslog

   action(type="mmdblookup"
          key="!clientip"
          mmdbFile="/etc/rsyslog.d/GeoLite2-City.mmdb"
          fields=["!continent!code", "!location"]
          reloadOnHup="off")

See also
--------
See also :doc:`../../configuration/modules/mmdblookup`.
