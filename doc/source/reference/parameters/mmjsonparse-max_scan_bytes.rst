.. _param-mmjsonparse-max_scan_bytes:
.. _mmjsonparse.parameter.max_scan_bytes:

max_scan_bytes
==============

.. index::
   single: mmjsonparse; max_scan_bytes
   single: max_scan_bytes
   single: scan limit

.. summary-start

Maximum number of bytes to scan when searching for JSON content in find-json mode.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/mmjsonparse`.

:Name: max_scan_bytes
:Scope: action
:Type: positive integer
:Default: 65536
:Required?: no
:Introduced: 8.2506.0

Description
-----------
Sets the maximum number of bytes to scan when searching for JSON objects in find-json mode. This prevents performance issues with very large messages by limiting the amount of data that will be examined.

When the scan limit is reached without finding a complete JSON object, the message is treated as non-JSON and processed through the fallback path (creating a simple JSON object with the original message as the "msg" field).

This parameter is only effective when mode="find-json".

Input usage
-----------
.. _mmjsonparse.parameter.max_scan_bytes-usage:

.. code-block:: rsyslog

   # Scan up to 32KB for JSON content
   action(type="mmjsonparse" mode="find-json" max_scan_bytes="32768")
   
   # Use default 64KB scan limit
   action(type="mmjsonparse" mode="find-json")

See also
--------
See also the :doc:`main mmjsonparse module documentation
<../../configuration/modules/mmjsonparse>`.