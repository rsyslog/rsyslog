omhdfs: Hadoop Filesystem Output Module
=======================================

**Module Name:    omhdfs**

**Available since:   ** 5.7.1

**Author:**\ Rainer Gerhards <rgerhards@adiscon.com>

**Description**:

This module supports writing message into files on Hadoop's HDFS file
system.

**Configuration Directives**:

-  **$OMHDFSFileName** [name]
    The name of the file to which the output data shall be written.
-  **$OMHDFSHost** [name]
    Name or IP address of the HDFS host to connect to.
-  **$OMHDFSPort** [name]
    Port on which to connect to the HDFS host.
-  **$OMHDFSDefaultTemplate** [name]
    Default template to be used when none is specified. This saves the
   work of specifying the same template ever and ever again. Of course,
   the default template can be overwritten via the usual method.

**Caveats/Known Bugs:**

Building omhdfs is a challenge because we could not yet find out how to
integrate Java properly into the autotools build process. The issue is
that HDFS is written in Java and libhdfs uses JNI to talk to it. That
requires that various system-specific environment options and pathes be
set correctly. At this point, we leave this to the user. If someone know
how to do it better, please drop us a line!

-  In order to build, you need to set these environment variables BEFORE
   running ./configure:

   -  JAVA\_INCLUDES - must have all include pathes that are needed to
      build JNI C programms, including the -I options necessary for gcc.
      An example is
       # export
      JAVA\_INCLUDES="-I/usr/lib/jvm/java-1.6.0-openjdk-1.6.0.0.x86\_64/include
      -I/usr/lib/jvm/java-1.6.0-openjdk-1.6.0.0.x86\_64/include/linux"
   -  JAVA\_LIBS - must have all library pathes that are needed to build
      JNI C programms, including the -l/-L options necessary for gcc. An
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

**Sample:**

$ModLoad omhdfs $OMHDFSFileName /var/log/logfile \*.\* :omhdfs: [`manual
index <manual.html>`_\ ] [`rsyslog site <http://www.rsyslog.com/>`_\ ]

This documentation is part of the `rsyslog <http://www.rsyslog.com/>`_
project.
Copyright © 2010 by `Rainer Gerhards <http://www.gerhards.net/rainer>`_
and `Adiscon <http://www.adiscon.com/>`_. Released under the GNU GPL
version 3 or higher.
