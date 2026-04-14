*************************
imhttp: HTTP input module
*************************

===========================  ===========
**Module Name:**             **imhttp**
**Author:**                  Nelson Yen
===========================  ===========


Purpose
=======

Provides the ability to receive adhoc and plaintext syslog messages via HTTP. The format of messages accepted,
depends on configuration. imhttp exposes the capabilities and the underlying options of the HTTP library
used, which currently is civetweb.

Civetweb documentation:

- `Civetweb User Manual <https://github.com/civetweb/civetweb/blob/master/docs/UserManual.md>`_
- `Civetweb Configuration Options <https://github.com/civetweb/civetweb/blob/master/docs/UserManual.md#configuration-options>`_

Notable Features
================

- :ref:`imhttp-statistic-counter`
- :ref:`imhttp-error-messages`
- :ref:`imhttp-prometheus-metrics`


Configuration Parameters
========================

.. note::

   Parameter names are case-insensitive.


Module Parameters
-----------------

Ports
^^^^^^^

.. csv-table::
   :header: "type", "mandatory", "format", "default"
   :widths: auto
   :class: parameter-table

   "string", "no", "ports", "8080"

Configures "listening_ports" in the civetweb library. This option may also be configured using the
liboptions_ (below) however, this option will take precedence.

The `ports` parameter accepts a comma-separated list of ports. To bind to a specific interface, use
the format ``IP:Port``. For IPv6 addresses, enclose the IP in brackets, e.g., ``[::1]:8080``.

- `Civetweb listening_ports <https://github.com/civetweb/civetweb/blob/master/docs/UserManual.md#listening_ports-8080>`_


documentroot
^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "mandatory", "format", "default"
   :widths: auto
   :class: parameter-table

   "string", "no", "none", "."

Configures "document_root" in the civetweb library. This option may also be configured using liboptions_, however
this option will take precedence.

- `Civetweb document_root <https://github.com/civetweb/civetweb/blob/master/docs/UserManual.md#document_root->`_


.. _liboptions:

liboptions
^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "mandatory", "format", "default"
   :widths: auto
   :class: parameter-table

   "string", "no", "none", "none"

Configures civetweb library "Options".

- `Civetweb Options <https://github.com/civetweb/civetweb/blob/master/docs/UserManual.md#options-from-civetwebc>`_


.. _imhttp-healthcheckpath:

healthCheckPath
^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "mandatory", "format", "default"
   :widths: auto
   :class: parameter-table

   "string", "no", "path that begins with '/'", "/healthz"

Configures the request path for a simple HTTP health probe that returns
``200`` when the module is running.
The endpoint is unauthenticated unless :ref:`healthCheckBasicAuthFile
<imhttp-healthcheckbasicauthfile>` or :ref:`healthCheckApiKeyFile
<imhttp-healthcheckapikeyfile>` is set. Otherwise, bind the server to
``localhost`` or use CivetWeb access controls if external access is not
desired.

Authentication Parameters
-------------------------

.. list-table::
   :widths: 30 70
   :header-rows: 1

   * - Parameter
     - Summary
   * - :ref:`param-imhttp-healthcheckbasicauthfile`
     - .. include:: ../../reference/parameters/imhttp-healthcheckbasicauthfile.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imhttp-healthcheckapikeyfile`
     - .. include:: ../../reference/parameters/imhttp-healthcheckapikeyfile.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imhttp-metricsbasicauthfile`
     - .. include:: ../../reference/parameters/imhttp-metricsbasicauthfile.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imhttp-metricsapikeyfile`
     - .. include:: ../../reference/parameters/imhttp-metricsapikeyfile.rst
        :start-after: .. summary-start
        :end-before: .. summary-end

.. _imhttp-metricspath:

metricsPath
^^^^^^^^^^^

.. csv-table::
   :header: "type", "mandatory", "format", "default"
   :widths: auto
   :class: parameter-table

   "string", "no", "path that begins with '/'", "/metrics"

Exposes rsyslog statistics in Prometheus text format at the specified
path. The endpoint emits counters such as ``imhttp_submitted_total`` and
``imhttp_failed_total``.
By default the endpoint does not enforce authentication, but it can be
protected with :ref:`metricsBasicAuthFile <imhttp-metricsbasicauthfile>`
or :ref:`metricsApiKeyFile <imhttp-metricsapikeyfile>`.
Leaving the default paths enabled creates user-visible URLs as soon as
the module is loaded, so review firewall and access-control settings.

Authentication Parameters
-------------------------

.. list-table::
   :widths: 30 70
   :header-rows: 1

   * - Parameter
     - Summary
   * - :ref:`param-imhttp-metricsbasicauthfile`
     - .. include:: ../../reference/parameters/imhttp-metricsbasicauthfile.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imhttp-metricsapikeyfile`
     - .. include:: ../../reference/parameters/imhttp-metricsapikeyfile.rst
        :start-after: .. summary-start
        :end-before: .. summary-end

Input Parameters
----------------

These parameters can be used with the "input()" statement. They apply to
the input they are specified with.


Endpoint
^^^^^^^^

.. csv-table::
   :header: "type", "mandatory", "format", "default"
   :widths: auto
   :class: parameter-table

   "string", "yes", "path that begins with '/' ", "none"

Sets a request path for an HTTP input. Path should always start with a '/'.


DisableLFDelimiter
^^^^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "mandatory", "format", "default"
   :widths: auto
   :class: parameter-table

   "binary", "no", "", "off"

By default LF is used to delimit msg frames, for data is sent in batches.
Set this to ‘on’ if this behavior is not needed.


Name
^^^^

.. csv-table::
   :header: "type", "mandatory", "format", "default"
   :widths: auto
   :class: parameter-table

   "string", "no", "", "imhttp"

Sets a name for the inputname property. If no name is set "imhttp"
is used by default. Setting a name is not strictly necessary, but can
be useful to apply filtering based on which input the message was
received from.


Ruleset
^^^^^^^

.. csv-table::
   :header: "type", "mandatory", "format", "default"
   :widths: auto
   :class: parameter-table

   "string", "no", "", "default ruleset"

Binds specified ruleset to this input. If not set, the default
ruleset is bound.


SupportOctetCountedFraming
^^^^^^^^^^^^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "mandatory", "format", "default"
   :widths: auto
   :class: parameter-table

   "binary", "no", "", "off"

Useful to send data using syslog style message framing, disabled by default. Message framing is described by `RFC 6587 <https://datatracker.ietf.org/doc/html/rfc6587#section-3.4.1>`_ .


RateLimit.Interval
^^^^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "mandatory", "format", "default"
   :widths: auto
   :class: parameter-table

   "integer", "no", "none", "0"

Specifies the rate-limiting interval in seconds. Set it to a number
of seconds to activate rate-limiting.


RateLimit.Burst
^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "mandatory", "format", "default"
   :widths: auto
   :class: parameter-table

   "integer", "no", "none", "10000"

Specifies the rate-limiting burst in number of messages.


RateLimit.Name
^^^^^^^^^^^^^^

.. include:: ../../reference/parameters/imhttp-ratelimit-name.rst
   :start-after: .. summary-start
   :end-before: .. summary-end


flowControl
^^^^^^^^^^^

.. csv-table::
   :header: "type", "mandatory", "format", "default"
   :widths: auto
   :class: parameter-table

   "binary", "no", "none", "on"

Flow control is used to throttle the sender if the receiver queue is
near-full preserving some space for input that can not be throttled.



addmetadata
^^^^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "mandatory", "format", "default"
   :widths: auto
   :class: parameter-table

   "binary", "no", "none", "off"

Enables metadata injection into `$!metadata` property. Currently, only header data is supported.
The following metadata will be injected into the following properties:

- `$!metadata!httpheaders`: HTTP header data will be injected here as key-value pairs. All header names will automatically be lowercased
  for case-insensitive access.

- `$!metadata!queryparams`: query parameters from the HTTP request will be injected here as key-value pairs. All header names will automatically be lowercased
  for case-insensitive access.


Authentication Parameters
-------------------------

.. list-table::
   :widths: 30 70
   :header-rows: 1

   * - Parameter
     - Summary
   * - :ref:`param-imhttp-basicauthfile`
     - .. include:: ../../reference/parameters/imhttp-basicauthfile.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-imhttp-apikeyfile`
     - .. include:: ../../reference/parameters/imhttp-apikeyfile.rst
        :start-after: .. summary-start
        :end-before: .. summary-end


.. _imhttp-statistic-counter:


Statistic Counter
=================

This plugin maintains global imhttp :doc:`statistics <../rsyslog_statistic_counter>`. The statistic's origin and name is "imhttp" and is
accumulated for all inputs. The statistic has the following counters:


-  **submitted** - Total number of messages successfully submitted for processing since startup.
-  **failed** - Total number of messages failed since startup, due to processing a request.
-  **discarded** - Total number of messages discarded since startup, due to rate limiting or similar.


.. _imhttp-prometheus-metrics:

Prometheus Metrics
==================

If :ref:`metricspath <imhttp-metricspath>` is configured (default
``/metrics``), imhttp exposes rsyslog statistics in the Prometheus
text exposition format. The endpoint provides counters such as
``imhttp_submitted_total``, ``imhttp_failed_total`` and
``imhttp_discarded_total``.

The handler sends a ``Content-Length`` header and closes the connection.
Proxies or load balancers must allow such responses. An ``imhttp_up`` gauge is exported
alongside the full rsyslog statistics. Name collisions with other
exporters are unlikely but should be documented in monitoring setups.
To expose only the metrics endpoint, load the module without configuring
any ``input()`` statements.


.. _imhttp-error-messages:

Error Messages
==============

When a message is to long it will be truncated and an error will show the remaining length of the message and the beginning of it. It will be easier to comprehend the truncation.


Caveats/Known Bugs
==================

-  module currently only a single HTTP instance, however multiple ports may be bound.


Examples
========

Example 1
---------

This sets up an HTTP server instance on port 8080 with two inputs.
One input path at '/postrequest', and another at '/postrequest2':

.. code-block:: none

   # ports=8080
   # document root='.'
   module(load="imhttp") # needs to be done just once

   # Input using default LF delimited framing
   # For example, the following HTTP request, with data body "Msg0001\nMsg0002\nMsg0003"
   ##
   # - curl -si http://localhost:$IMHTTP_PORT/postrequest -d $'Msg0001\nMsg0002\nMsg0003'
   ##
   # Results in the 3 message objects being submitted into rsyslog queues.
   # - Message object with `msg` property set to `Msg0001`
   # - Message object with `msg` property set to `Msg0002`
   # - Message object with `msg` property set to `Msg0003`

   input(type="imhttp"
         name="myinput1"
         endpoint="/postrequest"
         ruleset="postrequest_rs")

   # define 2nd input path, using octet-counted framing,
   # and routing to different ruleset
   input(type="imhttp"
         name="myinput2"
         endpoint="/postrequest2"
         SupportOctetCountedFraming="on"
         ruleset="postrequest_rs")

   # handle the messages in ruleset
   ruleset(name="postrequest_rs") {
      action(type="omfile" file="/var/log/http_messages" template="myformat")
   }


Example 2
---------

This sets up an HTTP server instance on ports 80 and 443s (use 's' to indicate ssl) with an input path at '/postrequest':

.. code-block:: none

   # ports=8080, 443 (ssl)
   # document root='.'
   module(load="imhttp" ports="8080,443s")
   input(type="imhttp"
         endpoint="/postrequest"
         ruleset="postrequest_rs")


Example 3
---------

Bind imhttp to specific IP addresses (e.g., localhost only) to prevent external access. This example
binds to IPv4 ``127.0.0.1`` on port 8080 and IPv6 ``[::1]`` on port 8081.

.. code-block:: none

   module(load="imhttp" ports="127.0.0.1:8080, [::1]:8081")
   input(type="imhttp" endpoint="/postrequest")



Example 4
---------

imhttp can also support the underlying options of `Civetweb <https://github.com/civetweb/civetweb/blob/master/docs/UserManual.md>`_ using the liboptions_ option.

.. code-block:: none

   module(load="imhttp"
          liboptions=[
            "error_log_file=my_log_file_path",
            "access_log_file=my_http_access_log_path",
          ])

   input(type="imhttp"
         endpoint="/postrequest"
         ruleset="postrequest_rs"
         )


Example 5
---------

Expose a Prometheus metrics endpoint alongside an input path:

.. code-block:: none

   module(load="imhttp" ports="8080" metricspath="/metrics")
   input(type="imhttp" endpoint="/postrequest")

   # scrape statistics
   # curl http://localhost:8080/metrics
   # HELP imhttp_up Indicates if the imhttp module is operational (1 for up, 0 for down).
   # TYPE imhttp_up gauge
   imhttp_up 1
   # HELP imhttp_submitted_total rsyslog stats: origin="imhttp" object="imhttp", counter="submitted"
   # TYPE imhttp_submitted_total counter
   imhttp_submitted_total 0

Example 6
---------

Expose only a Prometheus metrics endpoint secured with Basic
Authentication:

.. code-block:: none

   module(load="imhttp" ports="8080"
          metricspath="/metrics"
          metricsbasicauthfile="/etc/rsyslog/htpasswd")

   # scrape statistics with credentials
   # curl -u user:password http://localhost:8080/metrics

Example 7
---------

Protect an input endpoint with API-key authentication:

.. code-block:: none

   module(load="imhttp" ports="8080")
   input(type="imhttp"
         endpoint="/postrequest"
         apiKeyFile="/etc/rsyslog/imhttp-apikeys")

   # send logs with an API key
   # curl -H 'Authorization: ApiKey secret-token' \
   #      -d 'message payload' http://localhost:8080/postrequest

Example 8
---------

Allow either Basic or API-key authentication on the same input during a
migration:

.. code-block:: none

   module(load="imhttp" ports="8080")
   input(type="imhttp"
         endpoint="/postrequest"
         basicAuthFile="/etc/rsyslog/htpasswd"
         apiKeyFile="/etc/rsyslog/imhttp-apikeys")

Example 9
---------

Protect the metrics endpoint with an API key:

.. code-block:: none

   module(load="imhttp" ports="8080"
          metricsPath="/metrics"
          metricsApiKeyFile="/etc/rsyslog/imhttp-apikeys")

   # scrape statistics with an API key
   # curl -H 'Authorization: ApiKey secret-token' http://localhost:8080/metrics
