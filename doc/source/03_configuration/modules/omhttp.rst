********************************************
omhttp: HTTP Output Module
********************************************

===========================  ===========================================================================
**Module Name:**             **omhttp**
**Module Type:**             **contributed** - not maintained by rsyslog core team
**Current Maintainer:**       `Nelson Yen <https://github.com/n2yen/>`_
Original Author:             `Christian Tramnitz <https://github.com/ctramnitz/>`_
===========================  ===========================================================================


Purpose
=======

This module provides the ability to send messages over an HTTP REST interface.

This module supports sending messages in individual requests (the default), and batching multiple messages into a single request. Support for retrying failed requests is available in both modes. GZIP compression is configurable with the compress_ parameter. TLS encryption is configurable with the useHttps_ parameter and associated tls parameters.

In the default mode, every message is sent in its own HTTP request and it is a drop-in replacement for any other output module. In batch_ mode, the module implements several batch formatting options that are configurable via the batch.format_ parameter. Some additional attention to message formatting and retry_ strategy is required in this mode.

See the `Examples`_ section for some configuration examples.


Notable Features
================

- `Statistic Counter`_
- `Message Batching`_, supports several formats.
    - Newline concatenation, like the Elasticsearch bulk format.
    - JSON Array as a generic batching strategy.
    - Kafka REST Proxy format, to support sending data through the `Confluent Kafka REST API <https://docs.confluent.io/platform/current/kafka-rest/index.html>`_ to a Kafka cluster.

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

httpcontenttype
^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "word", "application/json; charset=utf-8", "no", "none"

The HTTP "Content-Type" header sent with each request. This parameter will override other defaults. If a batching mode is specified, the correct content type is automatically configured. The "Content-Type" header can also be configured using the httpheaders_ parameter, it should be configured in only one of the parameters.


httpheaderkey
^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "word", "none", "no", "none"

The header key. Currently only a single additional header/key pair is configurable, to specify multiple headers see the httpheaders_ parameter. This parameter along with httpheadervalue_ may be deprecated in the future.


httpheadervalue
^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "word", "none", "no", "none"

The header value for httpheaderkey_.

httpheaders
^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "array", "none", "no", "none"

An array of strings that defines a list of one or more HTTP headers to send with each message. Keep in mind that some HTTP headers are added using other parameters, "Content-Type" can be configured using httpcontenttype_ and "Content-Encoding: gzip" is added when using the compress_ parameter.

.. code-block:: text

    action(
        type="omhttp"
        ...
        httpheaders=[
            "X-Insert-Key: key",
            "X-Event-Source: logs"
        ]
        ...
    )


httpretrycodes
^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "array", "2xx status codes", "no", "none"

An array of strings that defines a list of one or more HTTP status codes that are retriable by the omhttp plugin. By default non-2xx HTTP status codes are considered retriable.


httpignorablecodes
^^^^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "array", "none", "no", "none"

An array of strings that defines a list of one or more HTTP status codes that are not retriable by the omhttp plugin.


proxyhost
^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "word", "none", "no", "none"

Configures `CURLOPT_PROXY` option, for which omhttp can use for HTTP request. For more details see libcurl docs on CURLOPT_PROXY


proxyport
^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "word", "none", "no", "none"

Configures `CURLOPT_PROXYPORT` option, for which omhttp can use for HTTP request. For more details see libcurl docs on CURLOPT_PROXYPORT


uid
^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "word", "none", "no", "none"

The username for basic auth.


pwd
^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "word", "none", "no", "none"

The password for the user for basic auth.


restpath
^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "word", "none", "no", "none"

The rest path you want to use. Do not include the leading slash character. If the full path looks like "localhost:5000/my/path", restpath should be "my/path".


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


restpathtimeout
^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "integer", "none", "no", "none"

Timeout value for the configured restpath. 


checkpath
^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "word", "none", "no", "none"

The health check path you want to use. Do not include the leading slash character. If the full path looks like "localhost:5000/my/path", checkpath should be "my/path".
When this parameter is set, omhttp utilizes this path to determine if it is safe to resume (from suspend mode) and communicates this status back to rsyslog core.
This parameter defaults to none, which implies that health checks are not needed, and it is always safe to resume from suspend mode.

**Important** - Note that it is highly recommended to set a valid health check path, as this allows omhttp to better determine whether it is safe to retry.
See the `rsyslog action queue documentation for more info <https://www.rsyslog.com/doc/v8-stable/configuration/actions.html>`_ regarding general rsyslog suspend and resume behavior.


batch
^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "binary", "off", "no", "none"

Batch and bulkmode do the same thing, bulkmode included for backwards compatibility. See the `Message Batching`_ section for a detailed breakdown of how batching is implemented.

This parameter activates batching mode, which queues messages and sends them as a single request. There are several related parameters that specify the format and size of the batch: they are batch.format_, batch.maxbytes_, and batch.maxsize_.

Note that rsyslog core is the ultimate authority on when a batch must be submitted, due to the way that batching is implemented. This plugin implements the `output plugin transaction interface <https://www.rsyslog.com/doc/v8-stable/development/dev_oplugins.html#output-plugin-transaction-interface>`_. There may be multiple batches in a single transaction, but a batch will never span multiple transactions. This means that if batch.maxsize_ or batch.maxbytes_ is set very large, you may never actually see batches hit this size. Additionally, the number of messages per transaction is determined by the size of the main, action, and ruleset queues as well.

Additionally, due to some open issues with rsyslog and the transaction interface, batching requires some nuanced retry_ configuration.


batch.format
^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "word", "newline", "no", "none"

This parameter specifies how to combine multiple messages into a single batch. Valid options are *newline* (default), *jsonarray*, *kafkarest*, and *lokirest*.

Each message on the "Inputs" line is the templated log line that is fed into the omhttp action, and the "Output" line describes the resulting payload sent to the configured HTTP server.

1. *newline* - Concatenates each message into a single string joined by newline ("\\n") characters. This mode is default and places no restrictions on the structure of the input messages.

.. code-block:: text

    Inputs: "message 1" "message 2" "message 3"
    Output: "message 1\nmessage2\nmessage3"

2. *jsonarray* - Builds a JSON array containing all messages in the batch. This mode requires that each message is parsable JSON, since the plugin parses each message as JSON while building the array.

.. code-block:: text

    Inputs: {"msg": "message 1"} {"msg"": "message 2"} {"msg": "message 3"}
    Output: [{"msg": "message 1"}, {"msg"": "message 2"}, {"msg": "message 3"}]

3. *kafkarest* - Builds a JSON object that conforms to the `Kafka Rest Proxy specification <https://docs.confluent.io/platform/current/kafka-rest/quickstart.html>`_. This mode requires that each message is parsable JSON, since the plugin parses each message as JSON while building the batch object.

.. code-block:: text

    Inputs: {"msg": "message 1"} {"msg"": "message 2"} {"msg": "message 3"}
    Output: {"records": [{"value": {"msg": "message 1"}}, {"value": {"msg": "message 2"}}, {"value": {"msg": "message 3"}}]}

4. *lokirest* - Builds a JSON object that conforms to the `Loki Rest specification <https://github.com/grafana/loki/blob/main/docs/sources/reference/loki-http-api.md#ingest-logs>`_. This mode requires that each message is parsable JSON, since the plugin parses each message as JSON while building the batch object. Additionally, the operator is responsible for providing index keys, and message values.

.. code-block:: text

    Inputs: {"stream": {"tag1":"value1"}, values:[[ "%timestamp%", "message 1" ]]} {"stream": {"tag2":"value2"}, values:[[ %timestamp%, "message 2" ]]}
    Output: {"streams": [{"stream": {"tag1":"value1"}, values:[[ "%timestamp%", "message 1" ]]},{"stream": {"tag2":"value2"}, values:[[ %timestamp%, "message 2" ]]}]}

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

batch.maxbytes and maxbytes do the same thing, maxbytes included for backwards compatibility.

This parameter specifies the maximum size in bytes for each batch.

template
^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "word", "StdJSONFmt", "no", "none"

The template to be used for the messages.

Note that in batching mode, this describes the format of *each* individual message, *not* the format of the resulting batch. Some batch modes require that a template produces valid JSON.


retry
^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "binary", "off", "no", "none"

This parameter specifies whether failed requests should be retried using the custom retry logic implemented in this plugin. Requests returning 5XX HTTP status codes are considered retriable. If retry is enabled, set retry.ruleset_ as well.

Note that retries are generally handled in rsyslog by setting action.resumeRetryCount="-1" (or some other integer), and the plugin lets rsyslog know it should start retrying by suspending itself. This is still the recommended approach in the 2 cases enumerated below when using this plugin. In both of these cases, the output plugin transaction interface is not used. That is, from rsyslog core's point of view, each message is contained in its own transaction.

1. Batching is off (batch="off")
2. Batching is on and the maximum batch size is 1 (batch="on" batch.maxsize="1")

This custom retry behavior is the result of a bug in rsyslog's handling of transaction commits. See `this issue <https://github.com/rsyslog/rsyslog/issues/2420>`_ for full details. Essentially, if rsyslog hands omhttp 4 messages, and omhttp batches them up but the request fails, rsyslog will only retry the LAST message that it handed the plugin, instead of all 4, even if the plugin returns the correct "defer commit" statuses for messages 1, 2, and 3. This means that omhttp cannot rely on action.resumeRetryCount for any transaction that processes more than a single message, and explains why the 2 above cases do work correctly.

It looks promising that issue will be resolved at some point, so this behavior can be revisited at that time.

retry.ruleset
^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "word", "none", "no", "none"

This parameter specifies the ruleset where this plugin should requeue failed messages if retry_ is on. This ruleset generally would contain another omhttp action instance.

**Important** - Note that the message that is queued on the retry ruleset is the templated output of the initial omhttp action. This means that no further templating should be done to messages inside this ruleset, unless retries should be templated differently than first-tries. An "echo template" does the trick here.

.. code-block:: text

   template(name="tpl_echo" type="string" string="%msg%")

This retry ruleset can recursively call itself as its own retry.ruleset to retry forever, but there is no timeout behavior currently implemented.

Alternatively, the omhttp action in the retry ruleset could be configured to support action.resumeRetryCount as explained above in the retry parameter section. The benefit of this approach is that retried messages still hit the server in a batch format (though with a single message in it), and the ability to configure rsyslog to give up after some number of resume attempts so as to avoid resource exhaustion.

Or, if some data loss or high latency is acceptable, do not configure retries with the retry ruleset itself. A single retry from the original ruleset might catch most failures, and errors from the retry ruleset could still be logged using the errorfile parameter and sent later on via some other process.


retry.addmetadata
^^^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "binary", "off", "no", "none"

When this option is enabled, omhttp will add the response metadata to: `$!omhttp!response`. There are 3 response metadata added: code, body, batch_index.



ratelimit.interval
^^^^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "integer", "600", "no", "none"

This parameter sets the rate limiting behavior for the retry.ruleset_. Specifies the interval in seconds onto which rate-limiting is to be applied. If more than ratelimit.burst messages are read during that interval, further messages up to the end of the interval are discarded. The number of messages discarded is emitted at the end of the interval (if there were any discards). Setting this to value zero turns off ratelimiting.

ratelimit.burst
^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "integer", "20000", "no", "none"

This parameter sets the rate limiting behavior for the retry.ruleset_. Specifies the maximum number of messages that can be emitted within the ratelimit.interval interval. For further information, see description there.


errorfile
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

compress
^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "binary", "off", "no", "none"

When switched to "on" each message will be compressed as GZIP using zlib's deflate compression algorithm.

A "Content-Encoding: gzip" HTTP header is added to each request when this feature is used. Set the compress.level_ for fine-grained control.

compress.level
^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "integer", "-1", "no", "none"

Specify the zlib compression level if compress_ is enabled. Check the `zlib manual <https://www.zlib.net/manual.html>`_ for further documentation.

"-1" is the default value that strikes a balance between best speed and best compression. "0" disables compression. "1" results in the fastest compression. "9" results in the best compression.

useHttps
^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "binary", "off", "no", "none"

When switched to "on" you will use `https` instead of `http`.


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

This plugin maintains global :doc:`statistics <../rsyslog_statistic_counter>` for omhttp that
accumulates all action instances. The statistic origin is named "omhttp" with following counters:

- **messages.submitted** - Number of messages submitted to omhttp. Messages resubmitted via a retry ruleset will be counted twice.

- **messages.success** - Number of messages successfully sent.

- **messages.fail** - Number of messages that omhttp failed to deliver for any reason.

- **messages.retry** - Number of messages that omhttp resubmitted for retry via the retry ruleset.

- **request.count** - Number of attempted HTTP requests.

- **request.success** - Number of successful HTTP requests. A successful request can return *any* HTTP status code.

- **request.fail** - Number of failed HTTP requests. A failed request is something like an invalid SSL handshake, or the server is not reachable. Requests returning 4XX or 5XX HTTP status codes are *not* failures.

- **request.status.success** - Number of requests returning 1XX or 2XX HTTP status codes.

- **request.status.fail** - Number of requests returning 3XX, 4XX, or 5XX HTTP status codes. If a requests fails (i.e. server not reachable) this counter will *not* be incremented.


Additionally, the following statistics can also be configured for a specific action instances. See `statsname`_ for more details.

- **requests.count** - Number of requests 

- **requests.status.0xx** - Number of failed requests. 0xx errors indicate request never reached destination.

- **requests.status.1xx** - Number of HTTP requests returing 1xx status codes

- **requests.status.2xx** - Number of HTTP requests returing 2xx status codes

- **requests.status.3xx** - Number of HTTP requests returing 3xx status codes

- **requests.status.4xx** - Number of HTTP requests returing 4xx status codes

- **requests.status.5xx** - Number of HTTP requests returing 5xx status codes

- **requests.bytes** - Total number of bytes sent - derived from CURLINFO_REQUEST_SIZE.

- **requests.time_ms** - Total accumulated request time in milliseconds - derived from CURLINFO_TOTAL_TIME.



Message Batching
================

See the batch.format_ section for some light examples of available batching formats.

Implementation
--------------

Here's the pseudocode of the batching algorithm used by omhttp. This section of code would run once per transaction.

.. code-block:: python

    Q = Queue()

    def submit(Q):                      # function to submit
        batch = serialize(Q)            # serialize according to configured batch.format
        result = post(batch)            # HTTP post serialized batch to server
        checkFailureAndRetry(Q, result) # check if post failed and pushed failed messages to configured retry.ruleset
        Q.empty()                       # reset for next batch


    while isActive(transaction):            # rsyslog manages the transaction
        message = receiveMessage()          # rsyslog sends us messages
        if wouldTriggerSubmit(Q, message):  # if this message puts us over maxbytes or maxsize
            submit(Q)                       # submit the current batch
        Q.push(message)                     # queue this message on the current batch

    submit(Q)   # transaction is over, submit what is currently in the queue


Walkthrough
-----------

This is a run through of a file tailing to omhttp scenario. Suppose we have a file called ``/var/log/my.log`` with this content..

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

    input(type="imfile" File="/var/log/my.log" ruleset="rs_omhttp" ... )

    # Produces JSON formatted payload
    template(name="tpl_omhttp_json" type="list") {
        constant(value="{")   property(name="msg"           outname="message"   format="jsonfr")
        constant(value=",")   property(name="hostname"      outname="host"      format="jsonfr")
        constant(value=",")   property(name="timereported"  outname="timestamp" format="jsonfr" dateFormat="rfc3339")
        constant(value="}")
    }

Our omhttp ruleset is configured to batch using the *jsonarray* format with 5 messages per batch, and to use a retry ruleset.


.. code-block:: text

    module(load="omhttp")

    ruleset(name="rs_omhttp") {
        action(
            type="omhttp"
            template="tpl_omhttp_json"
            batch="on"
            batch.format="jsonarray"
            batch.maxsize="5"
            retry="on"
            retry.ruleset="rs_omhttp_retry"
            ...
        )
    }

    call rs_omhttp

Each input message to this omhttp action is the output of ``tpl_omhttp_json`` with the following structure..

.. code-block:: text

    {"message": "001 message", "host": "localhost", "timestamp": "2018-12-28T21:14:13.840470+00:00"}

After 5 messages have been queued, and a batch submit is triggered, omhttp serializes the messages as a JSON array and attempts to post the batch to the server. At this point the payload on the wire looks like this..

.. code-block:: text

    [
        {"message": "001 message", "host": "localhost", "timestamp": "2018-12-28T21:14:13.000000+00:00"},
        {"message": "002 message", "host": "localhost", "timestamp": "2018-12-28T21:14:14.000000+00:00"},
        {"message": "003 message", "host": "localhost", "timestamp": "2018-12-28T21:14:15.000000+00:00"},
        {"message": "004 message", "host": "localhost", "timestamp": "2018-12-28T21:14:16.000000+00:00"},
        {"message": "005 message", "host": "localhost", "timestamp": "2018-12-28T21:14:17.000000+00:00"}
    ]

If the request fails, omhttp requeues each failed message onto the retry ruleset. However, recall that the inputs to the ``rs_omhttp`` ruleset are the rendered *outputs* of ``tpl_json_omhttp``, and therefore we *cannot* use the same template (and therefore the same action instance) to produce the retry messages. At this point, the ``msg`` rsyslog property is ``{"message": "001 message", "host": "localhost", "timestamp": "2018-12-28T21:14:13.000000+00:00"}`` instead of the original ``001 message``, and ``tpl_json_omhttp`` would render incorrect payloads.

Instead we define a simple template that echos its input..

.. code-block:: text

    template(name="tpl_echo" type="string" string="%msg%")

And assign it to the retry template..

.. code-block:: text

    ruleset(name="rs_omhttp_retry") {
        action(
            type="omhttp"
            template="tpl_echo"
            batch="on"
            batch.format="jsonarray"
            batch.maxsize="5"
            ...
        )
    }

And the destination is none the wiser! The *newline*, *jsonarray*, and *kafkarest* formats all behave in the same way with respect to their batching and retry behavior, and differ only in the format of the on-the-wire payload. The formats themselves are described in the batch.format_ section.

Examples
========

Example 1
---------

The following example is a basic usage, first the module is loaded and then
the action is used with a standard retry strategy.


.. code-block:: text

    module(load="omhttp")
    template(name="tpl1" type="string" string="{\"type\":\"syslog\", \"host\":\"%HOSTNAME%\"}")
    action(
        type="omhttp"
        server="127.0.0.1"
        serverport="8080"
        restpath="events"
        template="tpl1"
        action.resumeRetryCount="3"
    )

Example 2
---------

The following example is a basic batch usage with no retry processing.


.. code-block:: text

    module(load="omhttp")
    template(name="tpl1" type="string" string="{\"type\":\"syslog\", \"host\":\"%HOSTNAME%\"}")
    action(
        type="omhttp"
        server="127.0.0.1"
        serverport="8080"
        restpath="events"
        template="tpl1"
        batch="on"
        batch.format="jsonarray"
        batch.maxsize="10"
    )


Example 3
---------

The following example is a batch usage with a retry ruleset that retries forever


.. code-block:: text

    module(load="omhttp")

    template(name="tpl_echo" type="string" string="%msg%")
    ruleset(name="rs_retry_forever") {
        action(
            type="omhttp"
            server="127.0.0.1"
            serverport="8080"
            restpath="events"
            template="tpl_echo"

            batch="on"
            batch.format="jsonarray"
            batch.maxsize="10"

            retry="on"
            retry.ruleset="rs_retry_forever"
        )
    }

    template(name="tpl1" type="string" string="{\"type\":\"syslog\", \"host\":\"%HOSTNAME%\"}")
    action(
        type="omhttp"
        server="127.0.0.1"
        serverport="8080"
        restpath="events"
        template="tpl1"

        batch="on"
        batch.format="jsonarray"
        batch.maxsize="10"

        retry="on"
        retry.ruleset="rs_retry_forever"
    )

Example 4
---------

The following example is a batch usage with a couple retry options

.. code-block:: text

    module(load="omhttp")

    template(name="tpl_echo" type="string" string="%msg%")

    # This retry ruleset tries to send batches once then logs failures.
    # Error log could be tailed by rsyslog itself or processed by some
    # other program.
    ruleset(name="rs_retry_once_errorfile") {
        action(
            type="omhttp"
            server="127.0.0.1"
            serverport="8080"
            restpath="events"
            template="tpl_echo"

            batch="on"
            batch.format="jsonarray"
            batch.maxsize="10"

            retry="off"
            errorfile="/var/log/rsyslog/omhttp_errors.log"
        )
    }

    # This retry ruleset gives up trying to batch messages and instead always
    # uses a batch size of 1, relying on the suspend/resume mechanism to do
    # further retries if needed.
    ruleset(name="rs_retry_batchsize_1") {
        action(
            type="omhttp"
            server="127.0.0.1"
            serverport="8080"
            restpath="events"
            template="tpl_echo"

            batch="on"
            batch.format="jsonarray"
            batch.maxsize="1"
            action.resumeRetryCount="-1"
        )
    }

    template(name="tpl1" type="string" string="{\"type\":\"syslog\", \"host\":\"%HOSTNAME%\"}")
    action(
        type="omhttp"
        template="tpl1"

        ...

        retry="on"
        retry.ruleset="<some_retry_ruleset>"
    )

Example 5
---------

The following example is a batch action for pushing logs with checking, and queues to Loki.

.. code-block:: text

    module(load="omhttp")

    template(name="loki" type="string" string="{\"stream\":{\"host\":\"%HOSTNAME%\",\"facility\":\"%syslogfacility-text%\",\"priority\":\"%syslogpriority-text%\",\"syslogtag\":\"%syslogtag%\"},\"values\": [[ \"%timegenerated:::date-unixtimestamp%000000000\", \"%msg%\" ]]}")


    action(
        name="loki"
        type="omhttp"
        useHttps="off"
        server="localhost"
        serverport="3100"
        checkpath="ready"

        restpath="loki/api/v1/push"
        template="loki"
        batch.format="lokirest"
        batch="on"
        batch.maxsize="10"

        queue.size="10000" queue.type="linkedList"
        queue.workerthreads="3"
        queue.workerthreadMinimumMessages="1000"
        queue.timeoutWorkerthreadShutdown="500"
        queue.timeoutEnqueue="10000"
    )
