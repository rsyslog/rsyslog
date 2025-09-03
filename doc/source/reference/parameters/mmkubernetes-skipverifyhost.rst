.. _param-mmkubernetes-skipverifyhost:
.. _mmkubernetes.parameter.action.skipverifyhost:

skipverifyhost
==============

.. index::
   single: mmkubernetes; skipverifyhost
   single: skipverifyhost

.. summary-start

Skips verification of the Kubernetes API server hostname.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/mmkubernetes`.

:Name: skipverifyhost
:Scope: action
:Type: boolean
:Default: action=off
:Required?: no
:Introduced: at least 8.x, possibly earlier

Description
-----------
If `"on"`, this will set the curl `CURLOPT_SSL_VERIFYHOST` option to
`0`.  You are strongly discouraged to set this to `"on"`.  It is
primarily useful only for debugging or testing.

Action usage
------------
.. _param-mmkubernetes-action-skipverifyhost:
.. _mmkubernetes.parameter.action.skipverifyhost-usage:

.. code-block:: rsyslog

   action(type="mmkubernetes" skipVerifyHost="on")

See also
--------
See also :doc:`../../configuration/modules/mmkubernetes`.
