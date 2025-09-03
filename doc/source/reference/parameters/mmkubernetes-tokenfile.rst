.. _param-mmkubernetes-tokenfile:
.. _mmkubernetes.parameter.action.tokenfile:

tokenfile
=========

.. index::
   single: mmkubernetes; tokenfile
   single: tokenfile

.. summary-start

Reads the authentication token from the specified file.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/mmkubernetes`.

:Name: tokenfile
:Scope: action
:Type: word
:Default: action=none
:Required?: no
:Introduced: at least 8.x, possibly earlier

Description
-----------
The file containing the token to use to authenticate to the Kubernetes API
server.  One of `tokenfile` or `token` is required if Kubernetes is configured
with access control.  Example: `/etc/rsyslog.d/mmk8s.token`

Action usage
------------
.. _param-mmkubernetes-action-tokenfile:
.. _mmkubernetes.parameter.action.tokenfile-usage:

.. code-block:: rsyslog

   action(type="mmkubernetes" tokenFile="/etc/rsyslog.d/mmk8s.token")

See also
--------
See also :doc:`../../configuration/modules/mmkubernetes`.
