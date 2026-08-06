.. _param-mmkubernetes-tokenreloadinterval:
.. _mmkubernetes.parameter.action.tokenreloadinterval:

tokenreloadinterval
===================

.. index::
   single: mmkubernetes; tokenreloadinterval
   single: tokenreloadinterval

.. summary-start

How often to proactively re-read the bearer token from ``tokenfile``.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/mmkubernetes`.

:Name: tokenreloadinterval
:Scope: action
:Type: integer
:Default: 3600
:Required?: no
:Introduced: 8.2608.0

Description
-----------
How often, in seconds, to proactively re-read the bearer token from
:ref:`param-mmkubernetes-tokenfile`. The default is ``3600`` (one hour).

The token read from ``tokenfile`` is cached in the HTTP ``Authorization``
header when the worker starts. Kubernetes projected ServiceAccount tokens are
short-lived and rotated on disk by the kubelet (well before they expire), so a
long-running worker would otherwise keep sending the original token until it
becomes invalid, at which point metadata lookups fail with HTTP 401 until the
process is restarted. Re-reading the token periodically keeps the worker well
inside the token validity window.

Set this to ``0`` to disable the proactive reload. Regardless of this setting,
mmkubernetes also reloads the token from ``tokenfile`` and retries the request
once when a lookup returns HTTP 401, so a rotated token is picked up
automatically without a restart. This value must be 0 or greater.

This option has no effect when the inline :ref:`param-mmkubernetes-token`
parameter is used instead of ``tokenfile``.

Action usage
------------
.. _param-mmkubernetes-action-tokenreloadinterval:
.. _mmkubernetes.parameter.action.tokenreloadinterval-usage:

.. code-block:: rsyslog

   action(type="mmkubernetes" tokenReloadInterval="3600")

See also
--------
See also :doc:`../../configuration/modules/mmkubernetes`.
