.. _param-omhttp-httpignorablecodes:
.. _omhttp.parameter.module.httpignorablecodes:

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
:Scope: module
:Type: array
:Default: module=none
:Required?: no
:Introduced: Not specified

Description
-----------
An array of strings that defines a list of one or more HTTP status codes that are not retriable by the omhttp plugin.

Module usage
------------
.. _param-omhttp-module-httpignorablecodes:
.. _omhttp.parameter.module.httpignorablecodes-usage:

.. code-block:: rsyslog

   module(load="omhttp")

   action(
       type="omhttp"
       httpIgnorableCodes=["400", "404"]
   )

See also
--------
See also :doc:`../../configuration/modules/omhttp`.
