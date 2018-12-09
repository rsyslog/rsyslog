********************************************
omhttp: HTTP Output Module
********************************************

===========================  ===========================================================================
**Module Name:**             **omhttp**
**Author:**                  `Christian Tramnitz <https://github.com/ctramnitz/>`_
===========================  ===========================================================================

.. warning::

   This page is incomplete, if you want to contribute you can do this on
   github in the `rsyslog-doc repository <https://github.com/rsyslog/rsyslog-doc>`_.



Purpose
=======

This module provides the opportunity to send messages over a REST interface.


Notable Features
================


Configuration Parameters
========================

.. note::

   Parameter names are case-insensitive.


Action Parameters
-----------------

Server
^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "array", "localhost", "no", "none"

The server address you want to connect to.


Serverport
^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "integer", "443", "no", "none"

The port you want to connect to.


healthchecktimeout
^^^^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "integer", "3500", "no", "none"

The time after which the health check will time out in milliseconds.


httpheaderkey
^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "word", "none", "no", "none"

The header key.


httpheadervalue
^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "word", "none", "no", "none"

The header value.


uid
^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "word", "none", "no", "none"

The username.


pwd
^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "word", "none", "no", "none"

The password for the user


restpath
^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "word", "none", "no", "none"

The rest path you want to use.


dynrestpath
^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "binary", "off", "no", "none"

When this parameter is set to "on" you can specify a template name in the parameter
restpath instead of the actual path. This way you will be able to use dynamic rest
paths for your messages based on the template you are using.


bulkmode
^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "binary", "off", "no", "none"

With this parameter you can activate bulk mode, which will gather messages and send
them as one. How many messages are gathered can be specified in parameter maxbytes.


maxbytes
^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "Size", "10485760 (10MB)", "no", "none"

This parameter specifies the max length of a bulk.


useHttps
^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "binary", "off", "no", "none"

When switched to "on" you will use https instead of http.


errorfile
^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "word", "none", "no", "none"

Here you can set the name of a file where all errors will be written to.


erroronly
^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "binary", "off", "no", "none"

---


interleaved
^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "binary", "off", "no", "none"

---


template
^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "word", "StdJSONFmt", "no", "none"

The template to be used for the messages.


Examples
========

Example 1
---------

The following example is a basic usage, first the module is loaded and then
the action is used.


.. code-block:: none

    template(name="tpl1" type="string" string="{\"type\":\"syslog\", \"host\":\"%HOSTNAME%\"}")
    module(load="omhttp")
    action(type="omhttp" server="127.0.0.1" serverport="8080" restpath="events"
           template="tpl1")


