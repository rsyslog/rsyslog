.. _param-omhttp-uid:
.. _omhttp.parameter.module.uid:

uid
===

.. index::
   single: omhttp; uid
   single: uid

.. summary-start

Provides the username for HTTP basic authentication.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omhttp`.

:Name: uid
:Scope: module
:Type: word
:Default: module=none
:Required?: no
:Introduced: Not specified

Description
-----------
The username for basic auth.

Module usage
------------
.. _param-omhttp-module-uid:
.. _omhttp.parameter.module.uid-usage:

.. code-block:: rsyslog

   module(load="omhttp")

   action(
       type="omhttp"
       uid="api-user"
       pwd="secret"
   )

See also
--------
See also :doc:`../../configuration/modules/omhttp`.
