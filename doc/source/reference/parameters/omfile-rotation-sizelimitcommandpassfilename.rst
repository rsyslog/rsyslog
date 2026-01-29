.. _param-omfile-rotation-sizelimitcommandpassfilename:
.. _omfile.parameter.module.rotation-sizelimitcommandpassfilename:

rotation.sizeLimitCommandPassFileName
=====================================

.. index::
   single: omfile; rotation.sizeLimitCommandPassFileName
   single: rotation.sizeLimitCommandPassFileName

.. summary-start

Controls whether the current file name is passed as an argument to
`rotation.sizeLimitCommand`.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omfile`.

:Name: rotation.sizeLimitCommandPassFileName
:Scope: action
:Type: boolean
:Default: action=on
:Required?: no
:Introduced: 8.x (2026-01-28)

Description
-----------

When enabled, the file name that exceeded `rotation.sizeLimit` is appended as
the last argument to `rotation.sizeLimitCommand`. Any arguments specified as
part of `rotation.sizeLimitCommand` are retained and passed first.

Set to ``off`` to preserve scripts that expect no additional arguments.

Action usage
------------

.. _param-omfile-action-rotation-sizelimitcommandpassfilename:
.. _omfile.parameter.action.rotation-sizelimitcommandpassfilename:
.. code-block:: rsyslog

   action(type="omfile" rotation.sizeLimitCommandPassFileName="on")

See also
--------

See also :doc:`../../configuration/modules/omfile`.
