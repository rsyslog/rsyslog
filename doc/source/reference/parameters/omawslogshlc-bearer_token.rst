.. _param-omawslogshlc-bearer_token:
.. _omawslogshlc.parameter.action.bearer_token:

.. meta::
   :description: Reference for the omawslogshlc bearer_token parameter.
   :keywords: rsyslog, omawslogshlc, bearer_token, aws, cloudwatch, hlc

bearer_token
============

.. index::
   single: omawslogshlc; bearer_token
   single: bearer_token

.. summary-start

Specifies the bearer token for HLC endpoint authentication.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omawslogshlc`.

:Name: bearer_token
:Scope: action
:Type: string
:Default: none
:Required?: yes
:Introduced: Not specified

Description
-----------
``bearer_token`` is sent in the ``Authorization: Bearer <token>`` header
with each request. Tokens start with ``ACWL`` and are generated via the
CloudWatch Logs bearer token authentication setup.

Action usage
------------
.. _omawslogshlc.parameter.action.bearer_token-usage:

.. code-block:: rsyslog

   action(type="omawslogshlc" bearer_token="ACWL..." ...)

See also
--------
See also :doc:`../../configuration/modules/omawslogshlc`.
