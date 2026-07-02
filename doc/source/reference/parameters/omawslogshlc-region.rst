.. _param-omawslogshlc-region:
.. _omawslogshlc.parameter.action.region:

.. meta::
   :description: Reference for the omawslogshlc region parameter.
   :keywords: rsyslog, omawslogshlc, region, aws, cloudwatch

region
======

.. index::
   single: omawslogshlc; region
   single: region

.. summary-start

Specifies the AWS region for the CloudWatch Logs HLC endpoint.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omawslogshlc`.

:Name: region
:Scope: action
:Type: string
:Default: none
:Required?: yes
:Introduced: Not specified

Description
-----------
``region`` is used to construct the HLC endpoint URL:
``https://logs.<region>.amazonaws.com/services/collector/event``.
For example, ``us-west-2`` or ``eu-west-1``.

Action usage
------------
.. _omawslogshlc.parameter.action.region-usage:

.. code-block:: rsyslog

   action(type="omawslogshlc" region="us-west-2" ...)

See also
--------
See also :doc:`../../configuration/modules/omawslogshlc`.
