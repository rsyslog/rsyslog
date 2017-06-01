.. index:: ! mmdblookup

MaxMind/GeoIP DB lookup (mmdblookup)
####################################

================  ==================================
**Module Name:**  mmdblookup
**Author:**       `chenryn <rao.chenlin@gmail.com>`_
**Available:**    8.24+
================  ==================================

**Description**:

MaxMindDB is the new file format for storing information about IP addresses in a highly 
optimized, flexible database format. GeoIP2 Databases are available in the MaxMind DB format.

Plugin author claimed a MaxMindDB vs GeoIP speed around 4 to 6 times.

Module parameters
*****************
-  **container** - default "!iplocation" v8.28.0+

   Specifies the container to be used to store the fields ammended by
   mmdblookup.

Input parameters
****************

-  **key** - default none

   Name of field containing IP address.
   
-  **mmdbfile** - default none

   Location of Maxmind DB file.
   
-  **fields** - default none

   Fields that will be appended to processed message. The fields will
   always be appended in the container used by mmdblookup (which may be
   overriden by the "container" parameter on module load).
   
   By default, the maxmindb field name is used for variables. This can
   be overriden by specifying a custom name between colons at the 
   beginnig of the field name. As usual, bang signs denote path levels.
   So for example, if you want to extract "!city!names!en" but rename it
   to "cityname", you can use ":cityname:!city!names!en" as field name.


  
Examples
********

::

  # load module
  module( load="mmdblookup" )

  action( type="mmdblookup" mmdbfile="/etc/rsyslog.d/GeoLite2-City.mmdb" fields=["!continent!code","!location"] key="!clientip" )


The following example uses a custom container and custom field name::

  # load module
  module(load="mmdblookup" container="!geo_ip")

  action(type="mmdblookup" mmdbfile="/etc/rsyslog.d/GeoLite2-City.mmdb"
         fields=[":continent:!continent!code", ":loc:!location"]
	 key="!clientip"
	)


Building
********

To compile Rsyslog with mmdblookup you'll need to:

* install *libmaxminddb-devel* package
* set *--enable-mmdblookup* on configure
