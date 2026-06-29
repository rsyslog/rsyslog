.. _param-omhttp-httpignorablecodes:
.. _omhttp.parameter.input.httpignorablecodes:

httpignorablecodes
==================

.. index::
   single: omhttp; httpignorablecodes
   single: httpignorablecodes

.. summary-start

Lists HTTP status codes that omhttp never retries.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omhttp`.

:Name: httpignorablecodes
:Scope: input
:Type: array
:Default: input=none
:Required?: no
:Introduced: Not specified

Description
-----------
An array of strings that defines one or more HTTP status codes that omhttp
treats as successfully handled after the HTTP response is received. Use this
only when dropping the affected message is acceptable for the target system.
For example, a Splunk HEC deployment may choose to ignore a known permanent
HTTP 400 response after capturing the failed request in :ref:`param-omhttp-errorfile`.

Codes listed here override the normal failure result. They do not make the
remote service accept the data, and they do not replay the message later.

Input usage
-----------
.. _omhttp.parameter.input.httpignorablecodes-usage:

.. code-block:: rsyslog

   module(load="omhttp")

   action(
       type="omhttp"
       httpIgnorableCodes=["400", "404"]
   )

See also
--------
See also :doc:`../../configuration/modules/omhttp`.
