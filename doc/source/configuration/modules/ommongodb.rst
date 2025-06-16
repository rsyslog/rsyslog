********************************
ommongodb: MongoDB Output Module
********************************

===========================  ===========================================================================
**Module Name:**             **ommongodb**
**Author:**                  `Rainer Gerhards <https://rainer.gerhards.net/>`_ <rgerhards@adiscon.com>
===========================  ===========================================================================


Purpose
=======

This module provides native support for logging to MongoDB.


Configuration Parameters
========================

.. note::

   Parameter names are case-insensitive.


Action Parameters
-----------------

UriStr
^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "word", "none", "no", "none"

MongoDB connexion string, as defined by the MongoDB String URI Format (See: https://docs.mongodb.com/manual/reference/connection-string/). If uristr is defined, following directives will be ignored: server, serverport, uid, pwd.


SSL_Cert
^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "word", "none", "no", "none"

Absolute path to the X509 certificate you want to use for TLS client authentication. This is optional.


SSL_Ca
^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "word", "none", "no", "none"

Absolute path to the trusted X509 CA certificate that signed the mongoDB server certificate. This is optional.


db
^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "word", "syslog", "no", "none"

Database to use.


Collection
^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "word", "log", "no", "none"

Collection to use.


Allowed_Error_Codes
^^^^^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "array", "no", "no", "none"

The list of error codes returned by MongoDB you want ommongodb to ignore.
Please use the following format: allowed_error_codes=["11000","47"].


Template
^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "word", "OMSR_TPL_AS_MSG", "no", "none"

Template to use when submitting messages.

Note rsyslog contains a canned default template to write to the MongoDB.
It will be used automatically if no other template is specified to be
used. This template is:

.. code-block:: none

   template(name="BSON" type="string" string="\\"sys\\" : \\"%hostname%\\",
   \\"time\\" : \\"%timereported:::rfc3339%\\", \\"time\_rcvd\\" :
   \\"%timegenerated:::rfc3339%\\", \\"msg\\" : \\"%msg%\\",
   \\"syslog\_fac\\" : \\"%syslogfacility%\\", \\"syslog\_server\\" :
   \\"%syslogseverity%\\", \\"syslog\_tag\\" : \\"%syslogtag%\\",
   \\"procid\\" : \\"%programname%\\", \\"pid\\" : \\"%procid%\\",
   \\"level\\" : \\"%syslogpriority-text%\\"")


This creates the BSON document needed for MongoDB if no template is
specified. The default schema is aligned to CEE and project lumberjack.
As such, the field names are standard lumberjack field names, and
**not** `rsyslog property names <property_replacer.html>`_. When
specifying templates, be sure to use rsyslog property names as given in
the table. If you would like to use lumberjack-based field names inside
MongoDB (which probably is useful depending on the use case), you need
to select fields names based on the lumberjack schema. If you just want
to use a subset of the fields, but with lumberjack names, you can look
up the mapping in the default template. For example, the lumberjack
field "level" contains the rsyslog property "syslogpriority-text".


Examples
========


Write to Database
-----------------

The following sample writes all syslog messages to the database "syslog"
and into the collection "log" on mongoserver.example.com. The server is
being accessed under the account of "user" with password "pwd". Please note
that this syntax is deprecated by the "uristr" directive, as shown below.

.. code-block:: none

   module(load="ommongodb")
   action(type="ommongodb"
          server="mongoserver.example.com" db="syslog" collection="log"
          uid="user" pwd="pwd")


Write to mongoDB server with TLS and client authentication
----------------------------------------------------------

Another sample that uses the new "uristr" directives to connect to a TLS mongoDB server with TLS and client authentication.

.. code-block:: none

   module(load="ommongodb")
   action(type="ommongodb"
         uristr="mongodb://vulture:9091,vulture2:9091/?replicaset=Vulture&ssl=true"
         ssl_cert="/var/db/mongodb/mongod.pem"
         ssl_ca="/var/db/mongodb/ca.pem"
         db="logs" collection="syslog")


Deprecated Parameters
=====================

.. note::

   While these parameters are still accepted, they should no longer be used for newly created configurations.


Action Parameters
-----------------

Server
^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "word", "127.0.0.1", "no", "none"

Name or address of the MongoDB server.


ServerPorted
^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "word", "27017", "no", "none"

Permits to select a non-standard port for the MongoDB server. The
default is 0, which means the system default port is used. There is
no need to specify this parameter unless you know the server is
running on a non-standard listen port.


UID
^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "word", "none", "no", "none"

Logon userid used to connect to server. Must have proper permissions.


PWD
^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "word", "none", "no", "none"

The user's password.


