********************************************
omsplunkhec: HTTP Splunk HEC Output Module
********************************************

===========================  ===========================================================================
**Module Name:**             **omsplunkhec**
**Module Type:**             **contributed** - not maintained by rsyslog core team
**Current Maintainer:**       `Adrien GANDARIAS <https://github.com/shinigami35/>`_
===========================  ===========================================================================


Purpose
=======

This module provides the ability to send messages over an HTTP to a Splunk HEC server. This is a fork of the module "OMHTTP" with more function dedicated to Splunk HEC.

This module supports sending messages in individual requests (the default), and batching multiple messages into a single request. TLS encryption is configurable with the useTls_ parameter and associated tls parameters.

In the default mode, every message is sent in its own HTTP request and it is a drop-in replacement for any other output module. In batch_ mode, the module implements several batch formatting options that are not configurable because this plugin send in JSON Format, the only format accepted by an HEC Splunk Server. Some additional attention to message formatting is required in this mode.

See the `Examples`_ section for some configuration examples.


Notable Features
================

- `Statistic Counter`_

Configuration Parameters
========================

.. note::

   Parameter names are case-insensitive.


Action Parameters
-----------------

server
^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "array", "none", "no", "none"

The server address you want to connect to. This parameter is mandatory, if not server provided, the module cannot work.


port
^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "integer", "8088", "no", "none"

The port you want to connect to.


proxyhost
^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "word", "none", "no", "none"

Configures `CURLOPT_PROXY` option, for which omsplunkhec can use for HTTP request. For more details see libcurl docs on CURLOPT_PROXY


proxyport
^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "word", "none", "no", "none"

Configures `CURLOPT_PROXYPORT` option, for which omsplunkhec can use for HTTP request. For more details see libcurl docs on CURLOPT_PROXYPORT


token
^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "word", "none", "no", "none"

The token created by the HEC server.


batch
^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "binary", "off", "no", "none"

Batch and bulkmode do the same thing, bulkmode included for backwards compatibility. See the `Message Batching`_ section for a detailed breakdown of how batching is implemented.

This parameter activates batching mode, which queues messages and sends them as a single request. There are several related parameters that specify the format and size of the batch: they are batch.maxbytes_, and batch.maxsize_.

Note that rsyslog core is the ultimate authority on when a batch must be submitted, due to the way that batching is implemented. This plugin implements the `output plugin transaction interface <https://www.rsyslog.com/doc/v8-stable/development/dev_oplugins.html#output-plugin-transaction-interface>`_. There may be multiple batches in a single transaction, but a batch will never span multiple transactions. This means that if batch.maxsize_ or batch.maxbytes_ is set very large, you may never actually see batches hit this size. Additionally, the number of messages per transaction is determined by the size of the main, action, and ruleset queues as well.

batch.maxsize
^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "Size", "100", "no", "none"

This parameter specifies the maximum number of messages that will be sent in each batch.

batch.maxbytes
^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "Size", "10485760 (10MB)", "no", "none"

This parameter specifies the maximum size in bytes for each batch.

template
^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "word", "StdJSONFmt", "no", "none"

The template to be used for the messages.

I advice you to use this template :

.. code-block:: text

template(name="tpl_omsplunkhec_json_hec" type="list") {
    constant(value="{")
    property(name="rawmsg" outname="event" format="jsonf")
    constant(value="}")
}


errorFilename
^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "word", "none", "no", "none"

Here you can set the name of a file where all errors will be written to. Any request that returns a 4XX or 5XX HTTP code is recorded in the error file. Each line is JSON formatted with "request" and "response" fields, example pretty-printed below.

.. code-block:: text

    {
        "request": {
            "url": "https://example.com:443/path",
            "postdata": "mypayload"
        },
        "response" : {
            "status": 400,
            "message": "error string"
        }
    }

It is intended that a full replay of failed data is possible by processing this file.

useTls
^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "binary", "off", "no", "none"

When switched to "on" you will use https instead of http.


tls.cacert
^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "word", "none", "no", "none"

This parameter sets the path to the Certificate Authority (CA) bundle. Expects .pem format.

tls.mycert
^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "word", "none", "no", "none"

This parameter sets the path to the SSL client certificate. Expects .pem format.

tls.myprivkey
^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "word", "none", "no", "none"

The parameters sets the path to the SSL private key. Expects .pem format.

allowunsignedcerts
^^^^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "boolean", "off", "no", "none"

If `"on"`, this will set the curl `CURLOPT_SSL_VERIFYPEER` option to
`0`.  You are strongly discouraged to set this to `"on"`.  It is
primarily useful only for debugging or testing.

skipverifyhost
^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "boolean", "off", "no", "none"

If `"on"`, this will set the curl `CURLOPT_SSL_VERIFYHOST` option to
`0`.  You are strongly discouraged to set this to `"on"`.  It is
primarily useful only for debugging or testing.

reloadonhup
^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "binary", "off", "no", "none"

If this parameter is "on", the plugin will close and reopen any libcurl handles on a HUP signal. This option is primarily intended to enable reloading short-lived certificates without restarting rsyslog.


statsname
^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "word", "none", "no", "none"


The name assigned to statistics specific to this action instance. The supported set of
statistics tracked for this action instance are **submitted**, **acked**, **failures**.
See the `Statistic Counter`_ section for more details.


Statistic Counter
=================

This plugin maintains global :doc:`statistics <../rsyslog_statistic_counter>` for omsplunkhec that
accumulates all action instances. The statistic origin is named "omsplunkhec" with following counters:

- **request_submitted** - Number of messages submitted to omsplunkhec.

- **request_failed** - Number of messages that omsplunkhec failed to deliver for any reason.

- **request_count** - Number of requests.

- **request_succeeded** - Number of successful HTTP requests. A successful request can return *any* HTTP status code.

- **request_fail_serialized** - Number message that failed to be serialize into JSON

- **request_nb_msg** - Number message send to a HEC


Implementation
--------------

Here's the pseudocode of the batching algorithm used by omsplunkhec. This section of code would run once per transaction.

.. code-block:: python

    Q = Queue()

    def submit(Q):                      # function to submit
        batch = serialize(Q)            # serialize according to configured batch.format
        result = post(batch)            # http post serialized batch to server
        checkFailure(Q, result)         # check if post failed 
        Q.empty()                       # reset for next batch


    while isActive(transaction):            # rsyslog manages the transaction
        message = receiveMessage()          # rsyslog sends us messages
        if wouldTriggerSubmit(Q, message):  # if this message puts us over maxbytes or maxsize
            submit(Q)                       # submit the current batch
        Q.push(message)                     # queue this message on the current batch

    submit(Q)   # transaction is over, submit what is currently in the queue


Walkthrough
-----------

This is a run through of a file tailing to omsplunkhec scenario. Suppose we have a file called ``/var/log/my.log`` with this content..

.. code-block:: text

    001 message
    002 message
    003 message
    004 message
    005 message
    006 message
    007 message
    ...

We are tailing this using imfile and defining a template to generate a JSON payload...

.. code-block:: text

    input(type="imfile" File="/var/log/my.log" ruleset="rs_omsplunkhec" ... )

    # Produces JSON formatted payload
    template(name="tpl_omsplunkhec_json" type="list") {
        constant(value="{")   property(name="msg"           outname="message"   format="jsonfr")
        constant(value=",")   property(name="hostname"      outname="host"      format="jsonfr")
        constant(value=",")   property(name="timereported"  outname="timestamp" format="jsonfr" dateFormat="rfc3339")
        constant(value="}")
    }

Our omsplunkhec ruleset is configured to batch using the JSON format with 5 messages per batch.


.. code-block:: text

    module(load="omsplunkhec")

    ruleset(name="rs_omsplunkhec") {
        action(
            type="omsplunkhec"
            template="tpl_omsplunkhec_json"
            batch="on"
            batch.maxsize="5"
            ...
        )
    }

    call rs_omsplunkhec

Each input message to this omsplunkhec action is the output of ``tpl_omsplunkhec_json`` with the following structure..

.. code-block:: text

    {"message": "001 message", "host": "localhost", "timestamp": "2025-06-10T10:04:13.940470+00:00"}

After 5 messages have been queued, and a batch submit is triggered, omsplunkhec serializes the messages as a JSON array and attempts to post the batch to the server. At this point the payload on the wire looks like this..

.. code-block:: text

    
        {"message": "001 message", "host": "localhost", "timestamp": "2018-12-28T21:14:13.000000+00:00"}
        {"message": "002 message", "host": "localhost", "timestamp": "2018-12-28T21:14:14.000000+00:00"}
        {"message": "003 message", "host": "localhost", "timestamp": "2018-12-28T21:14:15.000000+00:00"}
        {"message": "004 message", "host": "localhost", "timestamp": "2018-12-28T21:14:16.000000+00:00"}
        {"message": "005 message", "host": "localhost", "timestamp": "2018-12-28T21:14:17.000000+00:00"}
    

Examples
========

Example 1
---------

The following example is a basic usage, first the module is loaded and then
the action is used with a standard retry strategy.


.. code-block:: text

    module(load="omsplunkhec")
	
	# Template Splunk HEC
	template(name="tpl_omsplunkhec_json_hec" type="list") {
		constant(value="{")
		property(name="$.hostname" outname="host" format="jsonfr")
		constant(value=",")
		property(name="$.sourcetype" outname="sourcetype" format="jsonfr")
		constant(value=",")
		property(name="$.index" outname="index" format="jsonfr")
		constant(value=",")
		property(name="rawmsg" outname="event" format="jsonf")
		constant(value="}")
	}
	
	ruleset(name="fwdomsplunkhec"){
        action(name="customhec"
               type="omsplunkhec"
               useTLS="off"
               server=["10.0.0.1", "10.0.0.2"]
               port="8088"
               errorFilename="/var/log/omsplunkhec_errors.log"
               token="XXXX-XXXX-XXXX-XXXX-XXXX"
	    
               restpath="services/collector"
               template="tpl_omsplunkhec_json_hec"
	    
               batch="on"
               batch.maxsize="200"
               batch.maxbytes="20971520"
	    
               queue.size="10000"
               queue.type="linkedList"
               queue.workerthreads="3"
               queue.workerthreadMinimumMessages="1000"
               queue.timeoutWorkerthreadShutdown="500"
               queue.timeoutEnqueue="10000"
        )

    }	
	
	ruleset(name="main"){

        if ($.data != "__UNKNOWN__") then {
            set $.sourcetype = "test";
            set $.index = "index_test";
            set $.hostname = $hostname;
	    
            if ($.index != "<NULL>" and $.sourcetype != "<NULL>" and $.hostname != "<NULL>") then {
                call fwdomsplunkhec
            }
            else {
                stop
            }
        } else {
            stop
        }
    }