************
HTTP-Request
************

Purpose
=======

http_request(str)

Performs an HTTP request to target and returns the result of said request.

.. note::

   this function is very slow and therefore we suggest using it only seldomly
   to insure adequate performance.


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


