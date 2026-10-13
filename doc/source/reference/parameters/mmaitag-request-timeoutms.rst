.. _param-mmaitag-request-timeoutms:
.. _mmaitag.parameter.action.request-timeoutms:

request.timeoutMs
=================

.. index::
   single: mmaitag; request.timeoutMs
   single: request.timeoutMs

.. summary-start

Sets the maximum total duration of one Gemini request in milliseconds.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/mmaitag`.

:Name: request.timeoutMs
:Scope: action
:Type: positive integer
:Default: 60000
:Maximum: 3600000
:Required?: no
:Introduced: 8.2610.0

Description
-----------
The complete request is aborted after this duration. The connection timeout
must not be greater than this value.

Action usage
------------

.. code-block:: rsyslog

   action(type="mmaitag" request.timeoutMs="120000")

See also
--------
* :doc:`../../configuration/modules/mmaitag`
