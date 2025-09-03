.. _param-mmkubernetes-allowunsignedcerts:
.. _mmkubernetes.parameter.action.allowunsignedcerts:

allowunsignedcerts
==================

.. index::
   single: mmkubernetes; allowunsignedcerts
   single: allowunsignedcerts

.. summary-start

Disables TLS peer certificate verification.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/mmkubernetes`.

:Name: allowunsignedcerts
:Scope: action
:Type: boolean
:Default: off
:Required?: no
:Introduced: at least 8.x, possibly earlier

Description
-----------
If `"on"`, this will set the curl `CURLOPT_SSL_VERIFYPEER` option to
`0`.  You are strongly discouraged to set this to `"on"`.  It is
primarily useful only for debugging or testing.

Action usage
------------
.. _param-mmkubernetes-action-allowunsignedcerts:
.. _mmkubernetes.parameter.action.allowunsignedcerts-usage:

.. code-block:: rsyslog

   action(type="mmkubernetes" allowUnsignedCerts="on")

See also
--------
See also :doc:`../../configuration/modules/mmkubernetes`.
