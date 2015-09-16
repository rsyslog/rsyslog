omhttpfs: Hadoop HTTPFS Output Module
============================================

===========================  ===========================================================================
**Module Name:**             **omhttpfs**
**Available Since:**         **8.10.0**
**Author:**                  `sskaje <https://sskaje.me/2014/12/omhttpfs-rsyslog-hdfs-output-plugin/>`_ <sskaje@gmail.com>
===========================  ===========================================================================

This module is an alternative to omhdfs via `Hadoop HDFS over HTTP <http://hadoop.apache.org/docs/current/hadoop-hdfs-httpfs/index.html>`_.

**Dependencies**

* libcurl

**Configure**
::
    ./configure --enable-omhttpfs

**Config options**

Legacy config **NOT** supported.

-  **host**
    HttpFS server host. Default: *127.0.0.1*
   
-  **port**
    HttpFS server port. Default: *14000*

-  **user**
    HttpFS auth user. Default: *hdfs*

-  **https** \ <on/**off**>
    Turn on if your HttpFS runs on HTTPS. Default: *off*

-  **file**
    File to write, or a template name.

-  **isdynfile** \ <on/**off**>
    Turn this on if your **file** is a template name. 
 
    See examples below.

-  **template**
    Format your message when writing to **file**. Default: *RSYSLOG_FileFormat*

**Examples**

::

    module(load="omhttpfs")
    template(name="hdfs_tmp_file" type="string" string="/tmp/%$YEAR%/test.log")
    template(name="hdfs_tmp_filecontent" type="string" string="%$YEAR%-%$MONTH%-%$DAY% %MSG% ==\n")
    local4.*    action(type="omhttpfs" host="10.1.1.161" port="14000" https="off" file="hdfs_tmp_file" isDynFile="on")
    local5.*    action(type="omhttpfs" host="10.1.1.161" port="14000" https="off" file="hdfs_tmp_file" isDynFile="on" template="hdfs_tmp_filecontent")
