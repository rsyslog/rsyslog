.. index:: ! mmdblookup

MaxMind/GeoIP DB lookup (mmdblookup)
####################################

================  ==================================
**Module Name:**  mmdblookup
**Author:**       `chenryn <rao.chenlin@gmail.com>`_
**Available:**    8.24
================  ==================================

**Description**:

MaxMindDB is the new file format for storing information about IP addresses in a highly 
optimized, flexible database format. GeoIP2 Databases are available in the MaxMind DB format.

Plugin author claimed a MaxMindDB vs GeoIP speed around 4 to 6 times.

Input parameters
****************

-  **key** - default none

   Name of field containing IP address.
   
-  **mmdbfile** - default none

   Location of Maxmind DB file.
   
-  **fields** - default none

   Fields that will be appended to processed message.

  
Examples
********

::

  # load módule
  module( load="mmdblookup" )

  action( type="mmdblookup" mmdbfile="/etc/rsyslog.d/GeoLite2-City.mmdb" fields=["!continent!code","!location"] key="!clientip" )

Building
********

To compile Rsyslog with mmdblookup you'll need to:

* install *libmaxminddb-devel* package
* set *--enable-mmdblookup* on configure
