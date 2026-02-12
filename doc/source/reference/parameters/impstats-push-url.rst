.. _param-impstats-push-url:
.. _impstats.parameter.module.push-url:

push.url
========

.. index::
   single: impstats; push.url
   single: push.url

.. summary-start

Specifies the Prometheus Remote Write endpoint URL to enable impstats push.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/impstats`.

:Name: push.url
:Scope: module
:Type: word
:Default: module=none
:Required?: no
:Introduced: 8.2602.0

Description
-----------
When set, impstats sends its statistics to the specified Remote Write endpoint.
Use an ``https://`` URL to enable TLS.

Module usage
------------
.. _impstats.parameter.module.push-url-usage:

.. code-block:: rsyslog

   module(load="impstats"
          push.url="http://localhost:8428/api/v1/write")

See also
--------
See also :doc:`../../configuration/modules/impstats`.
