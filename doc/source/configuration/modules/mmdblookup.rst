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

   Parameter names are case-insensitive; camelCase is recommended for readability.


Module Parameters
-----------------

.. list-table::
   :widths: 30 70
   :header-rows: 1

   * - Parameter
     - Summary
   * - :ref:`param-mmdblookup-container`
     - .. include:: ../../reference/parameters/mmdblookup-container.rst
        :start-after: .. summary-start
        :end-before: .. summary-end


Input Parameters
----------------

.. list-table::
   :widths: 30 70
   :header-rows: 1

   * - Parameter
     - Summary
   * - :ref:`param-mmdblookup-key`
     - .. include:: ../../reference/parameters/mmdblookup-key.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-mmdblookup-mmdbfile`
     - .. include:: ../../reference/parameters/mmdblookup-mmdbfile.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-mmdblookup-fields`
     - .. include:: ../../reference/parameters/mmdblookup-fields.rst
        :start-after: .. summary-start
        :end-before: .. summary-end


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


.. toctree::
   :hidden:

   ../../reference/parameters/mmdblookup-container
   ../../reference/parameters/mmdblookup-key
   ../../reference/parameters/mmdblookup-mmdbfile
   ../../reference/parameters/mmdblookup-fields


