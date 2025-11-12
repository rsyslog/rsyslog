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
An array of strings that defines a list of one or more HTTP status codes that are not retriable by the omhttp plugin.

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
