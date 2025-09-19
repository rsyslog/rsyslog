.. _param-mmdblookup-mmdbfile:
.. _mmdblookup.parameter.input.mmdbfile:

mmdbfile
========

.. index::
   single: mmdblookup; mmdbfile
   single: mmdbfile

.. summary-start

Specifies the path to the MaxMind DB file used for lookups.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/mmdblookup`.

:Name: mmdbfile
:Scope: input
:Type: string (word)
:Default: input=none
:Required?: yes
:Introduced: 8.24.0

Description
-----------
This parameter specifies the full path to the MaxMind DB file (for
example, ``/etc/rsyslog.d/GeoLite2-City.mmdb``) to be used for IP
lookups.

Input usage
-----------
.. _param-mmdblookup-input-mmdbfile:
.. _mmdblookup.parameter.input.mmdbfile-usage:

.. code-block:: rsyslog

   action(type="mmdblookup"
          key="!clientip"
          mmdbFile="/etc/rsyslog.d/GeoLite2-City.mmdb"
          fields=["!continent!code", "!location"])

See also
--------
See also :doc:`../../configuration/modules/mmdblookup`.
