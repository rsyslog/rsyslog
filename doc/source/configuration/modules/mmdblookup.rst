.. index:: ! mmdblookup

************************************
MaxMind/GeoIP DB lookup (mmdblookup)
************************************

================  ==================================
**Module Name:**  mmdblookup
**Author:**       `chenryn <rao.chenlin@gmail.com>`_
**Available:**    8.24+
================  ==================================


Purpose
=======

MaxMindDB is the new file format for storing information about IP addresses
in a highly optimized, flexible database format. GeoIP2 Databases are
available in the MaxMind DB format.

Plugin author claimed a MaxMindDB vs GeoIP speed around 4 to 6 times.


How to build the module
=======================

To compile Rsyslog with mmdblookup you'll need to:

* install *libmaxminddb-devel* package
* set *--enable-mmdblookup* on configure


Configuration Parameter
=======================

.. note::

   Parameter names are case-insensitive.


Module Parameters
-----------------

container
^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "word", "!iplocation", "no", "none"

.. versionadded:: 8.28.0

Specifies the container to be used to store the fields amended by
mmdblookup.


Input Parameters
----------------

key
^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "word", "none", "yes", "none"

Name of field containing IP address.


mmdbfile
^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "word", "none", "yes", "none"

Location of Maxmind DB file.


fields
^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "array", "none", "yes", "none"

Fields that will be appended to processed message. The fields will
always be appended in the container used by mmdblookup (which may be
overridden by the "container" parameter on module load).

By default, the maxmindb field name is used for variables. This can
be overridden by specifying a custom name between colons at the
beginning of the field name. As usual, bang signs denote path levels.
So for example, if you want to extract "!city!names!en" but rename it
to "cityname", you can use ":cityname:!city!names!en" as field name.


Examples
========

Minimum configuration
---------------------

This example shows the minimum configuration.

.. code-block:: none

   # load module
   module( load="mmdblookup" )

   action( type="mmdblookup" mmdbfile="/etc/rsyslog.d/GeoLite2-City.mmdb"
                fields=["!continent!code","!location"] key="!clientip" )


Custom container and field name
-------------------------------

The following example uses a custom container and custom field name

.. code-block:: none

   # load module
   module( load="mmdblookup" container="!geo_ip")

   action( type="mmdblookup" mmdbfile="/etc/rsyslog.d/GeoLite2-City.mmdb"
                fields=[":continent:!continent!code", ":loc:!location"]
                key="!clientip")


