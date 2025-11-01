.. _param-omhttp-pwd:
.. _omhttp.parameter.module.pwd:

pwd
===

.. index::
   single: omhttp; pwd
   single: pwd

.. summary-start

Provides the password for HTTP basic authentication.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omhttp`.

:Name: pwd
:Scope: module
:Type: word
:Default: module=none
:Required?: no
:Introduced: Not specified

Description
-----------
The password for the user for basic auth.

Module usage
------------
.. _omhttp.parameter.module.pwd-usage:

.. code-block:: rsyslog

   module(load="omhttp")

   action(
       type="omhttp"
       uid="api-user"
       pwd="s3cr3t!"
   )

See also
--------
See also :doc:`../../configuration/modules/omhttp`.
