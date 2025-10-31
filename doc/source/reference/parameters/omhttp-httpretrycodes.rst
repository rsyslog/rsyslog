.. _param-omhttp-httpretrycodes:
.. _omhttp.parameter.module.httpretrycodes:

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
:Scope: module
:Type: array
:Default: module=2xx status codes
:Required?: no
:Introduced: Not specified

Description
-----------
An array of strings that defines a list of one or more HTTP status codes that are retriable by the omhttp plugin. By default non-2xx HTTP status codes are considered retriable.

Module usage
------------
.. _param-omhttp-module-httpretrycodes:
.. _omhttp.parameter.module.httpretrycodes-usage:

.. code-block:: rsyslog

   module(load="omhttp")

   action(
       type="omhttp"
       httpRetryCodes=["500", "502", "503"]
   )

See also
--------
See also :doc:`../../configuration/modules/omhttp`.
