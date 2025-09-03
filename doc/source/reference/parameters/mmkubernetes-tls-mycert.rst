.. _param-mmkubernetes-tls-mycert:
.. _mmkubernetes.parameter.action.tls-mycert:

tls.mycert
==========

.. index::
   single: mmkubernetes; tls.mycert
   single: tls.mycert

.. summary-start

Specifies the client certificate for authenticating to Kubernetes.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/mmkubernetes`.

:Name: tls.mycert
:Scope: action
:Type: word
:Default: action=none
:Required?: no
:Introduced: at least 8.x, possibly earlier

Description
-----------
This is the full path and file name of the file containing the client cert for
doing client cert auth against Kubernetes.  This file is in PEM format.  For
example: `/etc/rsyslog.d/k8s-client-cert.pem`

Action usage
------------
.. _param-mmkubernetes-action-tls-mycert:
.. _mmkubernetes.parameter.action.tls-mycert-usage:

.. code-block:: rsyslog

   action(type="mmkubernetes" tls.myCert="/etc/rsyslog.d/k8s-client-cert.pem")

See also
--------
See also :doc:`../../configuration/modules/mmkubernetes`.
