.. _param-omhttp-checkpath:
.. _omhttp.parameter.input.checkpath:

checkpath
=========

.. index::
   single: omhttp; checkpath
   single: checkpath

.. summary-start

Defines the health-check endpoint that omhttp polls to decide when to resume after suspension.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omhttp`.

:Name: checkpath
:Scope: input
:Type: word
:Default: input=none
:Required?: no
:Introduced: Not specified

Description
-----------
The health check path you want to use. Do not include the leading slash character. If the full path looks like ``localhost:5000/my/path``, ``checkpath`` should be ``my/path``.

When this parameter is set, omhttp utilizes this path to determine if it is safe to resume (from suspend mode) and communicates this status back to rsyslog core.

This parameter defaults to ``none``, which implies that health checks are not needed, and it is always safe to resume from suspend mode.

**Important** - Note that it is highly recommended to set a valid health check path, as this allows omhttp to better determine whether it is safe to retry.

See the `rsyslog action queue documentation for more info <https://www.rsyslog.com/doc/v8-stable/configuration/actions.html>`_ regarding general rsyslog suspend and resume behavior.

Input usage
-----------
.. _omhttp.parameter.input.checkpath-usage:

.. code-block:: rsyslog

   module(load="omhttp")

   action(
       type="omhttp"
       checkPath="health"
   )

See also
--------
See also :doc:`../../configuration/modules/omhttp`.
