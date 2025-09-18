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
:Default: input=none
:Required?: yes
:Introduced: 8.24.0

Description
-----------
Fields that will be appended to processed message. The fields will
always be appended in the container used by mmdblookup (which may be
overridden by the "container" parameter on module load).

By default, the maxmindb field name is used for variables. This can
be overridden by specifying a custom name between colons at the
beginning of the field name. As usual, bang signs denote path levels.
So for example, if you want to extract "!city!names!en" but rename it
to "cityname", you can use ":cityname:!city!names!en" as field name.

Input usage
-----------
.. _param-mmdblookup-input-fields:
.. _mmdblookup.parameter.input.fields-usage:

.. code-block:: rsyslog

   action(type="mmdblookup"
          key="!clientip"
          mmdbFile="/etc/rsyslog.d/GeoLite2-City.mmdb"
          fields=[":continent:!continent!code", ":loc:!location"])

See also
--------
See also :doc:`../../configuration/modules/mmdblookup`.
