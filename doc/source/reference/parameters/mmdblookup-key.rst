.. _param-mmdblookup-key:
.. _mmdblookup.parameter.input.key:

key
===

.. index::
   single: mmdblookup; key
   single: key

.. summary-start

Identifies the message field that supplies the IP address to look up.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/mmdblookup`.

:Name: key
:Scope: input
:Type: string (word)
:Default: input=none
:Required?: yes
:Introduced: 8.24.0

Description
-----------
Name of field containing IP address. This parameter specifies the name
of the field in the message that contains the IP address to be looked
up. This is typically a JSON property obtained from a parsed message or
structured log.

Input usage
-----------
.. _param-mmdblookup-input-key:
.. _mmdblookup.parameter.input.key-usage:

.. code-block:: rsyslog

   action(type="mmdblookup"
          key="!clientip"
          mmdbFile="/etc/rsyslog.d/GeoLite2-City.mmdb"
          fields=["!continent!code", "!location"])

See also
--------
See also :doc:`../../configuration/modules/mmdblookup`.
