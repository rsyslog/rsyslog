.. _param-impstats-push-tls-insecureskipverify:
.. _impstats.parameter.module.push-tls-insecureskipverify:

push.tls.insecureSkipVerify
===========================

.. index::
   single: impstats; push.tls.insecureSkipVerify
   single: push.tls.insecureSkipVerify

.. summary-start

Disables TLS certificate verification for Remote Write (testing only).

.. summary-end

This parameter applies to :doc:`../../configuration/modules/impstats`.

:Name: push.tls.insecureSkipVerify
:Scope: module
:Type: boolean
:Default: module=off
:Required?: no
:Introduced: 8.2602.0

Description
-----------
When enabled, TLS peer and host verification are disabled. This should only be
used in testing environments.

Module usage
------------
.. _impstats.parameter.module.push-tls-insecureskipverify-usage:

.. code-block:: rsyslog

   module(load="impstats"
          push.tls.insecureSkipVerify="on")

See also
--------
See also :doc:`../../configuration/modules/impstats`.
