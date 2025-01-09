*************************
imhttp: http input module
*************************

===========================  ===========
**Module Name:**             **imhttp**
**Author:**                  Nelson Yen
===========================  ===========


Purpose
=======

Provides the ability to receive adhoc and plaintext syslog messages via http. The format of messages accepted,
depends on configuration. imhttp exposes the capabilities and the underlying options of the http library
used, which currently is civetweb.

Civetweb documentation:

- `Civetweb User Manual <https://github.com/civetweb/civetweb/blob/master/docs/UserManual.md>`_
- `Civetweb Configuration Options <https://github.com/civetweb/civetweb/blob/master/docs/UserManual.md#configuration-options>`_

Notable Features
================

- :ref:`imhttp-statistic-counter`
- :ref:`imhttp-error-messages`


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

Sets a request path for an http input. Path should always start with a '/'.


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

- `$!metadata!httpheaders`: http header data will be injected here as key-value pairs. All header names will automatically be lowercased
  for case-insensitive access.

- `$!metadata!queryparams`: query parameters from the http request will be injected here as key-value pairs. All header names will automatically be lowercased
  for case-insensitive access.


basicAuthFile
^^^^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "mandatory", "format", "default"
   :widths: auto
   :class: parameter-table

   "string", "no", "none", ""

Enables access control to this endpoint using http basic authentication. Option is disabled by default.
To enable it, set this option to an `htpasswd file`, which can be generated using a standard `htpasswd` tool.

See also:

- `HTTP Authorization <https://developer.mozilla.org/en-US/docs/Web/HTTP/Headers/Authorization>`_
- `HTTP Basic Authentication <https://developer.mozilla.org/en-US/docs/Web/HTTP/Authentication#basic_authentication_scheme>`_
- `htpasswd utility <https://httpd.apache.org/docs/2.4/programs/htpasswd.html>`_


.. _imhttp-statistic-counter:


basicAuthFile
^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "mandatory", "format", "default"
   :widths: auto
   :class: parameter-table

   "string", "no", "none", "none"

Configures an `htpasswd <https://httpd.apache.org/docs/2.4/programs/htpasswd.html>`_ file and enables `basic authentication <https://en.wikipedia.org/wiki/Basic_access_authentication>`_ on http request received on this input.
If this option is not set, basic authentication will not be enabled.


Statistic Counter
=================

This plugin maintains global imhttp :doc:`statistics <../rsyslog_statistic_counter>`. The statistic's origin and name is "imhttp" and is
accumulated for all inputs. The statistic has the following counters:


-  **submitted** - Total number of messages successfully submitted for processing since startup.
-  **failed** - Total number of messages failed since startup, due to processing a request.
-  **discarded** - Total number of messages discarded since startup, due to rate limiting or similar.


.. _imhttp-error-messages:

Error Messages
==============

When a message is to long it will be truncated and an error will show the remaining length of the message and the beginning of it. It will be easier to comprehend the truncation.


Caveats/Known Bugs
==================

-  module currently only a single http instance, however multiple ports may be bound.


Examples
========

Example 1
---------

This sets up an http server instance on port 8080 with two inputs.
One input path at '/postrequest', and another at '/postrequest2':

.. code-block:: none

   # ports=8080
   # document root='.'
   module(load="imhttp") # needs to be done just once

   # Input using default LF delimited framing
   # For example, the following http request, with data body "Msg0001\nMsg0002\nMsg0003"
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

This sets up an http server instance on ports 80 and 443s (use 's' to indicate ssl) with an input path at '/postrequest':

.. code-block:: none

   # ports=8080, 443 (ssl)
   # document root='.'
   module(load="imhttp" ports=8080,443s)
   input(type="imhttp"
         endpoint="/postrequest"
         ruleset="postrequest_rs")



Example 3
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
