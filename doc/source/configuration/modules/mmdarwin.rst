.. index:: ! mmdarwin

.. role:: json(code)
   :language: json

***************************
Darwin connector (mmdarwin)
***************************

================  ===========================================
**Module Name:**  **mmdarwin**
**Author:**       Guillaume Catto <guillaume.catto@advens.fr>,
                  Theo Bertin <theo.bertin@advens.fr>
================  ===========================================

Purpose
=======

Darwin is an open source Artificial Intelligence Framework for CyberSecurity. The mmdarwin module allows us to call Darwin in order to enrich our JSON-parsed logs with a score, and/or to allow Darwin to generate alerts.

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

The key name to use to store the returned data.

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

The Darwin filter socket path to use.


response
^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "word", \"no", "no", "none"

Tells the Darwin filter what to do next:

* :json:`"no"`: no response will be sent, nothing will be sent to next filter.
* :json:`"back"`: a score for the input will be returned by the filter, nothing will be forwarded to the next filter.
* :json:`"darwin"`: the data provided will be forwarded to the next filter (in the format specified in the filter's configuration), no response will be given to mmdarwin.
* :json:`"both"`: the filter will respond to mmdarwin with the input's score AND forward the data (in the format specified in the filter's configuration) to the next filter.

.. note::

   Please be mindful when setting this parameter, as the called filter will only forward data to the next configured filter if you ask the filter to do so with :json:`"darwin"` or :json:`"both"`, if a next filter if configured but you ask for a :json:`"back"` response, the next filter **WILL NOT** receive anything!

filtercode
^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "word", "0x00000000", "no", "none"

Each Darwin module has a unique filter code. For example, the code of the hostlookup filter is :json:`"0x66726570"`.
This code was mandatory but has now become obsolete. you can leave it as it is.

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

.. note::
    The order of the parameters is important. Thus, you have to be careful when providing the fields in the array.
    Refer to `Darwin documentation`_ to see what each filter requires as parameters.

.. _`Darwin documentation`: https://github.com/VultureProject/darwin/wiki

send_partial
^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "boolean", "off", "no", "none"

Whether to send to Darwin if not all :json:`"fields"` could be found in the message, or not.
All current Darwin filters required a strict number (and format) of parameters as input, so they will most likely not process the data if some fields are missing. This should be kept to "off", unless you know what you're doing.

For example, for the following log line:

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

   ["!from", "!data!status", "!this!field!is!not!in!message"]

the third field won't be found, so the call to Darwin will be dropped.


Configuration example
=====================

This example shows a possible configuration of mmdarwin.

.. code-block:: none

   module(load="imtcp")
   module(load="mmjsonparse")
   module(load="mmdarwin")

   input(type="imtcp" port="8042" Ruleset="darwin_ruleset")

   ruleset(name="darwin_ruleset") {
      action(type="mmjsonparse" cookie="")
      action(type="mmdarwin" socketpath="/path/to/reputation_1.sock" fields=["!srcip", "ATTACK;TOR"] key="reputation" response="back" filtercode="0x72657075")

      call darwin_output
   }

   ruleset(name="darwin_output") {
       action(type="omfile" file="/path/to/darwin_output.log")
   }
