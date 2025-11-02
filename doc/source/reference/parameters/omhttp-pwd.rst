.. _param-omhttp-pwd:
.. _omhttp.parameter.input.pwd:

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
:Scope: input
:Type: word
:Default: input=none
:Required?: no
:Introduced: Not specified

Description
-----------
The password for the user for basic auth.

Input usage
-----------
.. _omhttp.parameter.input.pwd-usage:

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
