.. SPDX-License-Identifier: Apache-2.0

************
HTTP-Request
************

Purpose
=======

http_request(str)

Performs an HTTP request to target and returns the result of said request.

.. note::

   this function is very slow and therefore we suggest using it only seldom
   to ensure adequate performance.


Limits and safety
=================

``http_request()`` runs synchronously while the rule is evaluated, so the
following fixed limits apply to every request:

- Only ``http://`` and ``https://`` URLs are accepted, also for redirects.
  Other schemes such as ``file://`` fail.
- The connection must be established within 10 seconds, and the whole
  request must complete within 30 seconds.
- Replies larger than 16 MiB are discarded.

If a request fails or hits a limit, the function returns the number ``0``
and logs an error. The logged URL omits user credentials, the query string,
and the fragment.

Keep the scheme, host, and path of the URL constant in the configuration.
Message content is untrusted: if you append it to the query string, as in
the example below, the sender controls that part of the request and can,
for example, add further query parameters.


Example
=======

The following example performs an HTTP request and writes the returned value
to a file.


.. code-block:: none

   module(load="../plugins/imtcp/.libs/imtcp")
   module(load="../plugins/fmhttp/.libs/fmhttp")
   input(type="imtcp" port="13514")

   template(name="outfmt" type="string" string="%$!%\n")

   if $msg contains "msgnum:" then {
   	set $.url = "http://www.rsyslog.com/testbench/echo-get.php?content=" & ltrim($msg);
   	set $!reply = http_request($.url);
   	action(type="omfile" file="rsyslog.out.log" template="outfmt")
   }


