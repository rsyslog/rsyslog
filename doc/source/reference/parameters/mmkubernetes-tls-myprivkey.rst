.. _param-mmkubernetes-tls-myprivkey:
.. _mmkubernetes.parameter.action.tls-myprivkey:

tls.myprivkey
=============

.. index::
   single: mmkubernetes; tls.myprivkey
   single: tls.myprivkey

.. summary-start

Specifies the unencrypted private key corresponding to ``tls.mycert``.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/mmkubernetes`.

:Name: tls.myprivkey
:Scope: action
:Type: word
:Default: action=none
:Required?: no
:Introduced: at least 8.x, possibly earlier

Description
-----------
This is the full path and file name of the file containing the private key
corresponding to the cert `tls.mycert` used for doing client cert auth against
Kubernetes.  This file is in PEM format, and must be unencrypted, so take
care to secure it properly.  For example: `/etc/rsyslog.d/k8s-client-key.pem`

Action usage
------------
.. _param-mmkubernetes-action-tls-myprivkey:
.. _mmkubernetes.parameter.action.tls-myprivkey-usage:

.. code-block:: rsyslog

   action(type="mmkubernetes" tls.myPrivKey="/etc/rsyslog.d/k8s-client-key.pem")

See also
--------
See also :doc:`../../configuration/modules/mmkubernetes`.
