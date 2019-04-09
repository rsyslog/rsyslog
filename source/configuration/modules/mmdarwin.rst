.. index:: ! mmdarwin

.. role:: json(code)
   :language: json

***************************
Darwin connector (mmdarwin)
***************************

================  ===========================================
**Module Name:**  **mmdarwin**
**Author:**       Guillaume Catto <guillaume.catto@advens.fr>
================  ===========================================

Purpose
=======

Darwin is an open source Artificial Intelligence Framework for CyberSecurity. The mmdarwin module allows us to call Darwin in order to enrich our JSON-parsed logs with a decision stored in a specific key.

How to build the module
=======================

To compile Rsyslog with mmdarwin you'll need to:

* set *--enable-mmdarwin* on configure

Configuration Parameter
=======================

Input Parameters
----------------

key
^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "word", "none", "yes", "none"

The key name used to enrich our logs.

For example, given the following log line:

.. code-block:: json

   {
       "from": "192.168.1.42",
       "date": "2012-12-21 00:00:00",
       "status": "200",
       "data": {
           "status": true,
           "message": "Request processed correctly"
       }
   }

and the :json:`"certitude"` key, the enriched log line would be:

.. code-block:: json
   :emphasize-lines: 9

   {
       "from": "192.168.1.42",
       "date": "2012-12-21 00:00:00",
       "status": "200",
       "data": {
           "status": true,
           "message": "Request processed correctly"
       },
       "certitude": 0
   }

where :json:`"certitude"` represents the score returned by Darwin.


socketpath
^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "word", "none", "yes", "none"

The Darwin filter socket path to call.


response
^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "word", "none", "yes", "none"

Tell the Darwin filter what to do with its decision:

* :json:`"no"`: no response will be sent
* :json:`"back"`: Darwin will send its decision to the caller
* :json:`"darwin"`: Darwin will send its decision to the next filter
* :json:`"both"`: Darwin will send its decision to both the caller and the next filter

filtercode
^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "word", "none", "yes", "none"

Each Darwin module has a unique filter code. For example, the code of the injection filter is :json:`"0x696E6A65"`. You need to provide a code corresponding to the filter you want to use.

fields
^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "array", "none", "yes", "none"

Array containing values to be sent to Darwin as parameters.

Two types of values can be set:

* if it starts with a bang (:json:`"!"`), mmdarwin will search in the JSON-parsed log line the associated value. You can search in subkeys as well: just add a bang to go to a deeper level.
* otherwise, the value is considered static, and will be forwarded directly to Darwin.

For example, given the following log line:

.. code-block:: json

   {
       "from": "192.168.1.42",
       "date": "2012-12-21 00:00:00",
       "status": "200",
       "data": {
           "status": true,
           "message": "Request processed correctly"
       }
   }

and the :json:`"fields"` array:

.. code-block:: none

   ["!from", "!data!status", "rsyslog"]

The parameters sent to Darwin would be :json:`"192.168.1.42"`, :json:`true` and :json:`"rsyslog"`.

Note that the order of the parameters is important. Thus, you have to be careful when providing the fields in the array.

Configuration example
=====================

This example shows a possible configuration of mmdarwin.

.. code-block:: none

   module(load="imtcp")
   module(load="mmjsonparse")
   module(load="mmdarwin")

   input(type="imtcp" port="8042" Ruleset="darwinruleset")

   ruleset(name="darwinruleset") {
      action(type="mmjsonparse" cookie="")
      action(type="mmdarwin" socketpath="/path/to/reputation_1.sock" fields=["!srcip", "ATTACK;TOR"] key="reputation" response="back" filtercode="0x72657075")

      call darwinoutput
   }

   ruleset(name="darwinoutput") {
       action(type="omfile" file="/path/to/darwin_output.log")
   }
