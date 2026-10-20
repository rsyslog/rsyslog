.. SPDX-License-Identifier: Apache-2.0

.. _param-mmaitag-request-lowspeedtime:
.. _mmaitag.parameter.action.request-lowspeedtime:

request.lowSpeedTime
====================

.. index::
   single: mmaitag; request.lowSpeedTime
   single: request.lowSpeedTime

.. summary-start

Sets how long a slow Gemini transfer may remain below the rate threshold.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/mmaitag`.

:Name: request.lowSpeedTime
:Scope: action
:Type: positive integer
:Default: 15
:Maximum: 3600
:Required?: no
:Introduced: 8.2610.0

Description
-----------
The unit is seconds. The request is aborted after the transfer remains below
``request.lowSpeedLimit`` for this duration.

Action usage
------------

.. code-block:: rsyslog

   action(type="mmaitag" request.lowSpeedTime="30")

See also
--------
* :doc:`../../configuration/modules/mmaitag`
