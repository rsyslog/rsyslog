.. _param-mmdblookup-mmdbfile:
.. _mmdblookup.parameter.input.mmdbfile:

Mmdbfile
========

.. index::
   single: mmdblookup; Mmdbfile
   single: Mmdbfile

.. summary-start

Specifies the path to the MaxMind DB file used for lookups.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/mmdblookup`.

:Name: Mmdbfile
:Scope: input
:Type: string (word)
:Default: input=none
:Required?: yes
:Introduced: 8.24.0

Description
-----------
Location of Maxmind DB file.

Input usage
-----------
.. _param-mmdblookup-input-mmdbfile:
.. _mmdblookup.parameter.input.mmdbfile-usage:

.. code-block:: rsyslog

   action(type="mmdblookup"
          key="!clientip"
          mmdbfile="/etc/rsyslog.d/GeoLite2-City.mmdb"
          fields=["!continent!code", "!location"])

See also
--------
See also :doc:`../../configuration/modules/mmdblookup`.
