.. _param-mmkubernetes-busyretryinterval:
.. _mmkubernetes.parameter.action.busyretryinterval:

busyretryinterval
=================

.. index::
   single: mmkubernetes; busyretryinterval
   single: busyretryinterval

.. summary-start

Sets the delay before retrying after a ``429 Busy`` response.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/mmkubernetes`.

:Name: busyretryinterval
:Scope: action
:Type: integer
:Default: 5
:Required?: no
:Introduced: at least 8.x, possibly earlier

Description
-----------
The number of seconds to wait before retrying operations to the Kubernetes API
server after receiving a `429 Busy` response.  The default `"5"` means that the
module will retry the connection every `5` seconds.  Records processed during
this time will _not_ have any additional metadata associated with them, so you
will need to handle cases where some of your records have all of the metadata
and some do not.

If you want to have rsyslog suspend the plugin until the Kubernetes API server
is available, set `busyretryinterval` to `"0"`.  This will cause the plugin to
return an error to rsyslog.

Action usage
------------
.. _param-mmkubernetes-action-busyretryinterval:
.. _mmkubernetes.parameter.action.busyretryinterval-usage:

.. code-block:: rsyslog

   action(type="mmkubernetes" busyRetryInterval="5")

See also
--------
See also :doc:`../../configuration/modules/mmkubernetes`.
