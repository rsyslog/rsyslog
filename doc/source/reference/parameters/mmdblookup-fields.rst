.. _param-mmdblookup-fields:
.. _mmdblookup.parameter.input.fields:

fields
======

.. index::
   single: mmdblookup; fields
   single: fields

.. summary-start

Defines the list of database fields whose values are appended to the message.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/mmdblookup`.

:Name: fields
:Scope: input
:Type: array (word)
:Default: none
:Required?: yes
:Introduced: 8.24.0

Description
-----------
This parameter specifies the fields that will be appended to processed
messages. The fields will always be appended in the container used by
mmdblookup (which may be overridden by the ``container`` parameter on module load).

By default, the lookup path itself is used as the name for the resulting
JSON property. This can be overridden by specifying a custom name. Use
the following syntax to control the resulting variable name and lookup
path:

* ``:customName:!path!to!field`` — specify the custom variable name
  enclosed in colons, followed by the MaxMind DB path.
* Bang signs (``!``) denote path levels within the database record.

For example, to extract ``!city!names!en`` but rename it to
``cityname``, use ``:cityname:!city!names!en`` as the field value.

Input usage
-----------
.. _mmdblookup.parameter.input.fields-usage:

.. code-block:: rsyslog

   action(type="mmdblookup"
          key="!clientip"
          mmdbFile="/etc/rsyslog.d/GeoLite2-City.mmdb"
          fields=[":continent:!continent!code", ":loc:!location"])

See also
--------
See also :doc:`../../configuration/modules/mmdblookup`.
