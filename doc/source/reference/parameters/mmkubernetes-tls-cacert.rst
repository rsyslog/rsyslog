.. _param-mmkubernetes-tls-cacert:
.. _mmkubernetes.parameter.action.tls-cacert:

tls.cacert
==========

.. index::
   single: mmkubernetes; tls.cacert
   single: tls.cacert

.. summary-start

Specifies the CA certificate used to verify the Kubernetes API server.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/mmkubernetes`.

:Name: tls.cacert
:Scope: action
:Type: word
:Default: action=none
:Required?: no
:Introduced: at least 8.x, possibly earlier

Description
-----------
Full path and file name of file containing the CA cert of the
Kubernetes API server cert issuer.  Example: `/etc/rsyslog.d/mmk8s-ca.crt`.
This parameter is not mandatory if using an `http` scheme instead of `https` in
`kubernetesurl`, or if using `allowunsignedcerts="yes"`.

Action usage
------------
.. _param-mmkubernetes-action-tls-cacert:
.. _mmkubernetes.parameter.action.tls-cacert-usage:

.. code-block:: rsyslog

   action(type="mmkubernetes" tls.caCert="/etc/rsyslog.d/mmk8s-ca.crt")

See also
--------
See also :doc:`../../configuration/modules/mmkubernetes`.
