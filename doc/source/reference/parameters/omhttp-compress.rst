.. _param-omhttp-compress:
.. _omhttp.parameter.module.compress:

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
:Scope: module
:Type: boolean
:Default: module=off
:Required?: no
:Introduced: Not specified

Description
-----------
When switched to ``on`` each message will be compressed as GZIP using zlib's deflate compression algorithm.

A ``Content-Encoding: gzip`` HTTP header is added to each request when this feature is used. Set the :ref:`param-omhttp-compress-level` for fine-grained control.

Module usage
------------
.. _param-omhttp-module-compress:
.. _omhttp.parameter.module.compress-usage:

.. code-block:: rsyslog

   module(load="omhttp")

   action(
       type="omhttp"
       compress="on"
       compress.level="6"
   )

See also
--------
See also :doc:`../../configuration/modules/omhttp`.
