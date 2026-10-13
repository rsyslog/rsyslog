.. _param-mmaitag-request-connecttimeoutms:
.. _mmaitag.parameter.action.request-connecttimeoutms:

request.connectTimeoutMs
========================

.. index::
   single: mmaitag; request.connectTimeoutMs
   single: request.connectTimeoutMs

.. summary-start

Sets the Gemini connection-establishment timeout in milliseconds.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/mmaitag`.

:Name: request.connectTimeoutMs
:Scope: action
:Type: positive integer
:Default: 10000
:Maximum: 3600000 and the configured ``request.timeoutMs``
:Required?: no
:Introduced: 8.2610.0

Description
-----------
The request is aborted when a provider connection cannot be established within
this duration.

Action usage
------------

.. code-block:: rsyslog

   action(type="mmaitag" request.connectTimeoutMs="20000")

See also
--------
* :doc:`../../configuration/modules/mmaitag`
