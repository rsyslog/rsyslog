.. _param-omhttp-compress:
.. _omhttp.parameter.input.compress:

compress
========

.. index::
   single: omhttp; compress
   single: compress

.. summary-start

Enables GZIP compression of each HTTP payload sent by omhttp.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omhttp`.

:Name: compress
:Scope: input
:Type: boolean
:Default: input=off
:Required?: no
:Introduced: Not specified

Description
-----------
When switched to ``on`` each message will be compressed as GZIP using zlib's deflate compression algorithm.

A ``Content-Encoding: gzip`` HTTP header is added to each request when this feature is used. Set the :ref:`param-omhttp-compress-level` for fine-grained control.

Input usage
-----------
.. _omhttp.parameter.input.compress-usage:

.. code-block:: rsyslog

   module(load="omhttp")

   action(
       type="omhttp"
       compress="on"
       compressLevel="6"
   )

See also
--------
See also :doc:`../../configuration/modules/omhttp`.
