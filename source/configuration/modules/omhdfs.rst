***************************************
omhdfs: Hadoop Filesystem Output Module
***************************************

===========================  ===========================================================================
**Module Name:**             **omhdfs**
**Author:**                  `Rainer Gerhards <https://rainer.gerhards.net/>`_ <rgerhards@adiscon.com>
===========================  ===========================================================================


Purpose
=======

This module supports writing message into files on Hadoop's HDFS file
system.


Configuration Parameters
========================

.. note::

   Parameter names are case-insensitive.


|FmtObsoleteName| Directives
----------------------------

.. csv-table::
   :header: "|FmtObsoleteName| directive", "type", "default", "mandatory"
   :widths: auto
   :class: parameter-table

   "``$OMHDFSFileName``", "word", "none", "no"

The name of the file to which the output data shall be written.

.. csv-table::
   :header: "|FmtObsoleteName| directive", "type", "default", "mandatory"
   :widths: auto
   :class: parameter-table

   "``$OMHDFSHost``", "word", "default", "no"

Name or IP address of the HDFS host to connect to.

.. csv-table::
   :header: "|FmtObsoleteName| directive", "type", "default", "mandatory"
   :widths: auto
   :class: parameter-table

   "``$OMHDFSPort``", "integer", "0", "no"

Port on which to connect to the HDFS host.

.. csv-table::
   :header: "|FmtObsoleteName| directive", "type", "default", "mandatory"
   :widths: auto
   :class: parameter-table

   "``$OMHDFSDefaultTemplate``", "word", "RSYSLOG_FileFormat", "no"

Default template to be used when none is specified. This saves the work of
specifying the same template ever and ever again. Of course, the default
template can be overwritten via the usual method.


Caveats/Known Bugs
==================

Building omhdfs is a challenge because we could not yet find out how to
integrate Java properly into the autotools build process. The issue is
that HDFS is written in Java and libhdfs uses JNI to talk to it. That
requires that various system-specific environment options and paths be
set correctly. At this point, we leave this to the user. If someone knows
how to do it better, please drop us a line!

-  In order to build, you need to set these environment variables BEFORE
   running ./configure:

   -  JAVA\_INCLUDES - must have all include paths that are needed to
      build JNI C programs, including the -I options necessary for gcc.
      An example is
      # export
      JAVA\_INCLUDES="-I/usr/lib/jvm/java-1.6.0-openjdk-1.6.0.0.x86\_64/include
      -I/usr/lib/jvm/java-1.6.0-openjdk-1.6.0.0.x86\_64/include/linux"
   -  JAVA\_LIBS - must have all library paths that are needed to build
      JNI C programs, including the -l/-L options necessary for gcc. An
      example is
      # export export
      JAVA\_LIBS="-L/usr/java/jdk1.6.0\_21/jre/lib/amd64
      -L/usr/java/jdk1.6.0\_21/jre/lib/amd64/server -ljava -ljvm
      -lverify"

-  As of HDFS architecture, you must make sure that all relevant
   environment variables (the usual Java stuff and HADOOP's home
   directory) are properly set.
-  As it looks, libhdfs makes Java throw exceptions to stdout. There is
   no known work-around for this (and it usually should not case any
   troubles.


Examples
========

Example 1
---------

.. code-block:: none

   $ModLoad omhdfs
   $OMHDFSFileName /var/log/logfile \*.\* :omhdfs:


