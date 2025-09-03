.. _param-mmkubernetes-token:
.. _mmkubernetes.parameter.action.token:

token
=====

.. index::
   single: mmkubernetes; token
   single: token

.. summary-start

Specifies the authentication token string.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/mmkubernetes`.

:Name: token
:Scope: action
:Type: word
:Default: none
:Required?: no
:Introduced: at least 8.x, possibly earlier

Description
-----------
The token to use to authenticate to the Kubernetes API server.  One of `token`
or :ref:`param-mmkubernetes-tokenfile` is required if Kubernetes is configured with access control.
Example: `UxMU46ptoEWOSqLNa1bFmH`

Action usage
------------
.. _param-mmkubernetes-action-token:
.. _mmkubernetes.parameter.action.token-usage:

.. code-block:: rsyslog

   action(type="mmkubernetes" token="UxMU46ptoEWOSqLNa1bFmH")

See also
--------
See also :doc:`../../configuration/modules/mmkubernetes`.
