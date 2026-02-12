.. meta::
   :description: Configure the maximum HTTP response size stored by omhttp.
   :keywords: rsyslog, omhttp, replymaxbytes, http, response size

.. _param-omhttp-replymaxbytes:
.. _omhttp.parameter.input.replymaxbytes:

replymaxbytes
=============

.. index::
   single: omhttp; replymaxbytes
   single: replymaxbytes

.. summary-start

Limits how much HTTP response data omhttp stores per request.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omhttp`.

:Name: replymaxbytes
:Scope: input
:Type: Size
:Default: input=1048576 (1MB)
:Required?: no
:Introduced: Not specified

Description
-----------
``replymaxbytes`` caps the number of bytes retained from an HTTP response.
Set this to bound memory usage when servers return large response bodies.
Use ``0`` to disable the limit.

Input usage
-----------
.. _omhttp.parameter.input.replymaxbytes-usage:

.. code-block:: rsyslog

   module(load="omhttp")

   action(
       type="omhttp"
       replyMaxBytes="512k"
   )

See also
--------
See also :doc:`../../configuration/modules/omhttp`.
