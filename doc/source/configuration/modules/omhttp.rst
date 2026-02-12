**************************
omhttp: HTTP Output Module
**************************

===========================  ===========================================================================
**Module Name:**             **omhttp**
**Module Type:**             **contributed** - not maintained by rsyslog core team
**Current Maintainer:**       `Nelson Yen <https://github.com/n2yen/>`_
Original Author:             `Christian Tramnitz <https://github.com/ctramnitz/>`_
===========================  ===========================================================================


Purpose
=======

This module provides the ability to send messages over an HTTP REST interface.

This module supports sending messages in individual requests (the default), and batching multiple messages into a single request. Support for retrying failed requests is available in both modes. GZIP compression is configurable with the :ref:`param-omhttp-compress` parameter. TLS encryption is configurable with the :ref:`param-omhttp-usehttps` parameter and associated tls parameters.

In the default mode, every message is sent in its own HTTP request and it is a drop-in replacement for any other output module. In :ref:`param-omhttp-batch` mode, the module implements several batch formatting options that are configurable via the :ref:`param-omhttp-batch-format` parameter. Some additional attention to message formatting and :ref:`param-omhttp-retry` strategy is required in this mode.

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

   Parameter names are case-insensitive; camelCase is recommended for readability.


Input Parameters
----------------

.. list-table::
   :widths: 30 70
   :header-rows: 1

   * - Parameter
     - Summary
   * - :ref:`param-omhttp-server`
     - .. include:: ../../reference/parameters/omhttp-server.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-omhttp-serverport`
     - .. include:: ../../reference/parameters/omhttp-serverport.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-omhttp-healthchecktimeout`
     - .. include:: ../../reference/parameters/omhttp-healthchecktimeout.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-omhttp-healthchecktimedelay`
     - .. include:: ../../reference/parameters/omhttp-healthchecktimedelay.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-omhttp-httpcontenttype`
     - .. include:: ../../reference/parameters/omhttp-httpcontenttype.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-omhttp-httpheaderkey`
     - .. include:: ../../reference/parameters/omhttp-httpheaderkey.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-omhttp-httpheadervalue`
     - .. include:: ../../reference/parameters/omhttp-httpheadervalue.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-omhttp-httpheaders`
     - .. include:: ../../reference/parameters/omhttp-httpheaders.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-omhttp-httpretrycodes`
     - .. include:: ../../reference/parameters/omhttp-httpretrycodes.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-omhttp-httpignorablecodes`
     - .. include:: ../../reference/parameters/omhttp-httpignorablecodes.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-omhttp-proxyhost`
     - .. include:: ../../reference/parameters/omhttp-proxyhost.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-omhttp-proxyport`
     - .. include:: ../../reference/parameters/omhttp-proxyport.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-omhttp-uid`
     - .. include:: ../../reference/parameters/omhttp-uid.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-omhttp-pwd`
     - .. include:: ../../reference/parameters/omhttp-pwd.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-omhttp-restpath`
     - .. include:: ../../reference/parameters/omhttp-restpath.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-omhttp-dynrestpath`
     - .. include:: ../../reference/parameters/omhttp-dynrestpath.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-omhttp-restpathtimeout`
     - .. include:: ../../reference/parameters/omhttp-restpathtimeout.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-omhttp-checkpath`
     - .. include:: ../../reference/parameters/omhttp-checkpath.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-omhttp-batch`
     - .. include:: ../../reference/parameters/omhttp-batch.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-omhttp-batch-format`
     - .. include:: ../../reference/parameters/omhttp-batch-format.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-omhttp-batch-maxsize`
     - .. include:: ../../reference/parameters/omhttp-batch-maxsize.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-omhttp-batch-maxbytes`
     - .. include:: ../../reference/parameters/omhttp-batch-maxbytes.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-omhttp-replymaxbytes`
     - .. include:: ../../reference/parameters/omhttp-replymaxbytes.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-omhttp-template`
     - .. include:: ../../reference/parameters/omhttp-template.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-omhttp-retry`
     - .. include:: ../../reference/parameters/omhttp-retry.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-omhttp-retry-ruleset`
     - .. include:: ../../reference/parameters/omhttp-retry-ruleset.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-omhttp-retry-addmetadata`
     - .. include:: ../../reference/parameters/omhttp-retry-addmetadata.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-omhttp-ratelimit-interval`
     - .. include:: ../../reference/parameters/omhttp-ratelimit-interval.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-omhttp-ratelimit-burst`
     - .. include:: ../../reference/parameters/omhttp-ratelimit-burst.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-omhttp-errorfile`
     - .. include:: ../../reference/parameters/omhttp-errorfile.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-omhttp-compress`
     - .. include:: ../../reference/parameters/omhttp-compress.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-omhttp-compress-level`
     - .. include:: ../../reference/parameters/omhttp-compress-level.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-omhttp-usehttps`
     - .. include:: ../../reference/parameters/omhttp-usehttps.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-omhttp-tls-cacert`
     - .. include:: ../../reference/parameters/omhttp-tls-cacert.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-omhttp-tls-mycert`
     - .. include:: ../../reference/parameters/omhttp-tls-mycert.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-omhttp-tls-myprivkey`
     - .. include:: ../../reference/parameters/omhttp-tls-myprivkey.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-omhttp-allowunsignedcerts`
     - .. include:: ../../reference/parameters/omhttp-allowunsignedcerts.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-omhttp-skipverifyhost`
     - .. include:: ../../reference/parameters/omhttp-skipverifyhost.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-omhttp-reloadonhup`
     - .. include:: ../../reference/parameters/omhttp-reloadonhup.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-omhttp-statsname`
     - .. include:: ../../reference/parameters/omhttp-statsname.rst
        :start-after: .. summary-start
        :end-before: .. summary-end

.. toctree::
   :hidden:

   ../../reference/parameters/omhttp-server
   ../../reference/parameters/omhttp-serverport
   ../../reference/parameters/omhttp-healthchecktimeout
   ../../reference/parameters/omhttp-healthchecktimedelay
   ../../reference/parameters/omhttp-httpcontenttype
   ../../reference/parameters/omhttp-httpheaderkey
   ../../reference/parameters/omhttp-httpheadervalue
   ../../reference/parameters/omhttp-httpheaders
   ../../reference/parameters/omhttp-httpretrycodes
   ../../reference/parameters/omhttp-httpignorablecodes
   ../../reference/parameters/omhttp-proxyhost
   ../../reference/parameters/omhttp-proxyport
   ../../reference/parameters/omhttp-uid
   ../../reference/parameters/omhttp-pwd
   ../../reference/parameters/omhttp-restpath
   ../../reference/parameters/omhttp-dynrestpath
   ../../reference/parameters/omhttp-restpathtimeout
   ../../reference/parameters/omhttp-checkpath
   ../../reference/parameters/omhttp-batch
   ../../reference/parameters/omhttp-batch-format
   ../../reference/parameters/omhttp-batch-maxsize
   ../../reference/parameters/omhttp-batch-maxbytes
   ../../reference/parameters/omhttp-replymaxbytes
   ../../reference/parameters/omhttp-template
   ../../reference/parameters/omhttp-retry
   ../../reference/parameters/omhttp-retry-ruleset
   ../../reference/parameters/omhttp-retry-addmetadata
   ../../reference/parameters/omhttp-ratelimit-interval
   ../../reference/parameters/omhttp-ratelimit-burst
   ../../reference/parameters/omhttp-errorfile
   ../../reference/parameters/omhttp-compress
   ../../reference/parameters/omhttp-compress-level
   ../../reference/parameters/omhttp-usehttps
   ../../reference/parameters/omhttp-tls-cacert
   ../../reference/parameters/omhttp-tls-mycert
   ../../reference/parameters/omhttp-tls-myprivkey
   ../../reference/parameters/omhttp-allowunsignedcerts
   ../../reference/parameters/omhttp-skipverifyhost
   ../../reference/parameters/omhttp-reloadonhup
   ../../reference/parameters/omhttp-statsname


statsbysenders
^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "binary", "off", "no", "none"


This parameter configures `omhttp` to generate statistics on a per-destination-server basis, rather than per action instance. The destination servers are specified in the `server` parameter.
if this is enabled, the name of the stats will be: "<instance name>(<server name>)".
if this is disabled, the name of the stats will be: "<instance name>(ALL)".


profile
^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "word", "none", "no", "none"


The profile allow user to use a defined profile supported by this module.
The list of the current supported profile is :

1. loki : allow Rsyslog to send data to the loki endpoint "loki/api/v1/push"
2. hec:splunk:event : allow Rsyslog to send data to the Splunk HEC endpoint "event". This profile use a custom template to format logs into json :

.. code-block:: text

    {"event":"%rawmsg:::json%"}

3. hec:splunk:raw : allow Rsyslog to send data to the Splunk HEC endpoint "raw". This profile use a custom template to format logs :

.. code-block:: text

    %rawmsg:::drop-last-lf%\n


Other name set up here is ignored.

token
^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "word", "none", "no", "none"


The token is necessary when you use the profile "hec:splunk:event" or "hec:splunk:raw". The token to set up is given by when you create a Splunk HEC.
This value is ignored if you use other profile.


Statistic Counter
=================

This plugin maintains global :doc:`statistics <../rsyslog_statistic_counter>` for omhttp that
accumulates all action instances. The statistic origin is named "omhttp" with following counters:

- **messages.submitted** - Number of messages submitted to omhttp. Messages resubmitted via a retry ruleset will be counted twice.

- **messages.success** - Number of messages successfully sent.

- **messages.fail** - Number of messages that omhttp failed to deliver for any reason.

- **messages.retry** - Number of messages that omhttp resubmitted for retry via the retry ruleset.

- **request.success** - Number of successful HTTP requests. A successful request can return *any* HTTP status code.

- **request.fail** - Number of failed HTTP requests. A failed request is something like an invalid SSL handshake, or the server is not reachable. Requests returning 4XX or 5XX HTTP status codes are *not* failures.

- **request.status.success** - Number of requests returning 1XX or 2XX HTTP status codes.

- **request.status.fail** - Number of requests returning 3XX, 4XX, or 5XX HTTP status codes. If a requests fails (i.e. server not reachable) this counter will *not* be incremented.


Additionally, the following statistics can also be configured for a specific action instances. See :ref:`param-omhttp-statsname` for more details.

- **requests.count** - Number of requests

- **requests.status.0xx** - Number of failed requests. 0xx errors indicate request never reached destination.

- **requests.status.1xx** - Number of HTTP requests returning 1xx status codes

- **requests.status.2xx** - Number of HTTP requests returning 2xx status codes

- **requests.status.3xx** - Number of HTTP requests returning 3xx status codes

- **requests.status.4xx** - Number of HTTP requests returning 4xx status codes

- **requests.status.5xx** - Number of HTTP requests returning 5xx status codes

- **requests.bytes** - Total number of bytes sent - derived from CURLINFO_REQUEST_SIZE.

- **requests.time_ms** - Total accumulated request time in milliseconds - derived from CURLINFO_TOTAL_TIME.



Message Batching
================

See the :ref:`param-omhttp-batch-format` section for some light examples of available batching formats.

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

And the destination is none the wiser! The *newline*, *jsonarray*, and *kafkarest* formats all behave in the same way with respect to their batching and retry behavior, and differ only in the format of the on-the-wire payload. The formats themselves are described in the :ref:`param-omhttp-batch-format` section.

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
