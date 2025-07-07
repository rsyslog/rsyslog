*************************************
omhttpfs: Hadoop HTTPFS Output Module
*************************************

===========================  ===========================================================================
**Module Name:**             **omhttpfs**
**Available Since:**         **8.10.0**
**Author:**                  `sskaje <https://sskaje.me/2014/12/omhttpfs-rsyslog-hdfs-output-plugin/>`_ <sskaje@gmail.com>
===========================  ===========================================================================


Purpose
=======

This module is an alternative to omhdfs via `Hadoop HDFS over HTTP <http://hadoop.apache.org/docs/current/hadoop-hdfs-httpfs/index.html>`_.


Dependencies
============

* libcurl


Configuration Parameters
========================

.. note::

   Parameter names are case-insensitive.


Action Parameters
-----------------

Host
^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "word", "127.0.0.1", "no", "none"

HttpFS server host.


Port
^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "integer", "14000", "no", "none"

HttpFS server port.


User
^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "word", "hdfs", "no", "none"

HttpFS auth user.


https
^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "binary", "off", "no", "none"

Turn on if your HttpFS runs on HTTPS.


File
^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "word", "none", "yes", "none"

File to write, or a template name.


isDynFile
^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "binary", "off", "no", "none"

Turn this on if your **file** is a template name.
See examples below.


Template
^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "word", "RSYSLOG_FileFormat", "no", "none"

Format your message when writing to **file**. Default: *RSYSLOG_FileFormat*


Configure
=========

.. code-block:: none

    ./configure --enable-omhttpfs


Examples
========

Example 1
---------

.. code-block:: none

    module(load="omhttpfs")
    template(name="hdfs_tmp_file" type="string" string="/tmp/%$YEAR%/test.log")
    template(name="hdfs_tmp_filecontent" type="string" string="%$YEAR%-%$MONTH%-%$DAY% %MSG% ==\n")
    local4.*    action(type="omhttpfs" host="10.1.1.161" port="14000" https="off" file="hdfs_tmp_file" isDynFile="on")
    local5.*    action(type="omhttpfs" host="10.1.1.161" port="14000" https="off" file="hdfs_tmp_file" isDynFile="on" template="hdfs_tmp_filecontent")


