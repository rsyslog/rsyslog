.. _param-omhttp-httpretrycodes:
.. _omhttp.parameter.input.httpretrycodes:

httpretrycodes
==============

.. index::
   single: omhttp; httpretrycodes
   single: httpretrycodes

.. summary-start

Lists HTTP status codes that omhttp treats as retriable errors.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omhttp`.

:Name: httpretrycodes
:Scope: input
:Type: array
:Default: input=none
:Required?: no
:Introduced: Not specified

Description
-----------
An array of strings that defines one or more HTTP status codes that are
retriable by the omhttp plugin in addition to its built-in retry behavior.
By default, transport failures, HTTP 429, and HTTP 5xx responses are retried;
ordinary HTTP 3xx and 4xx responses are treated as permanent data failures.
Use this parameter when a deployment wants a specific response, such as a
temporary HTTP 403 from an authorization workflow, to follow the retry path.

Input usage
-----------
.. _omhttp.parameter.input.httpretrycodes-usage:

.. code-block:: rsyslog

   module(load="omhttp")

   action(
       type="omhttp"
       httpRetryCodes=["500", "502", "503"]
   )

See also
--------
See also :doc:`../../configuration/modules/omhttp`.
