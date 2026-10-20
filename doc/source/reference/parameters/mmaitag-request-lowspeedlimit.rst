.. SPDX-License-Identifier: Apache-2.0

.. _param-mmaitag-request-lowspeedlimit:
.. _mmaitag.parameter.action.request-lowspeedlimit:

request.lowSpeedLimit
=====================

.. index::
   single: mmaitag; request.lowSpeedLimit
   single: request.lowSpeedLimit

.. summary-start

Sets the minimum acceptable Gemini transfer rate in bytes per second.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/mmaitag`.

:Name: request.lowSpeedLimit
:Scope: action
:Type: positive integer
:Default: 1
:Maximum: 1048576
:Required?: no
:Introduced: 8.2610.0

Description
-----------
If the transfer remains below this rate for ``request.lowSpeedTime`` seconds,
the request is aborted.

Action usage
------------

.. code-block:: rsyslog

   action(type="mmaitag" request.lowSpeedLimit="128")

See also
--------
* :doc:`../../configuration/modules/mmaitag`
