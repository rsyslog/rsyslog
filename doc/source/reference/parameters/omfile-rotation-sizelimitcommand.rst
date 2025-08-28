.. _param-omfile-rotation-sizelimitcommand:
.. _omfile.parameter.module.rotation-sizelimitcommand:

rotation.sizeLimitCommand
=========================

.. index::
   single: omfile; rotation.sizeLimitCommand
   single: rotation.sizeLimitCommand

.. summary-start

Configures the script invoked when `rotation.sizeLimit` is exceeded.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omfile`.

:Name: rotation.sizeLimitCommand
:Scope: action
:Type: string
:Default: action=(empty)
:Required?: no
:Introduced: at least 5.x, possibly earlier

Description
-----------

This sets the command executed when a file hits the configured size limit.
Use together with :ref:`param-omfile-rotation-sizelimit`.

Action usage
------------

.. _param-omfile-action-rotation-sizelimitcommand:
.. _omfile.parameter.action.rotation-sizelimitcommand:
.. code-block:: rsyslog

   action(type="omfile" rotation.sizeLimitCommand="...")

Notes
-----

- Legacy documentation referred to the type as ``binary``; the value is a command string.

Legacy names (for reference)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Historic names/directives for compatibility. Do not use in new configs.

.. _omfile.parameter.legacy.outchannel:

- $outchannel — maps to rotation.sizeLimitCommand (status: legacy)

.. index::
   single: omfile; $outchannel
   single: $outchannel

See also
--------

See also :doc:`../../configuration/modules/omfile`.
