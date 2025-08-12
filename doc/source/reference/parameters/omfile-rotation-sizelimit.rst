.. _param-omfile-rotation-sizelimit:
.. _omfile.parameter.module.rotation-sizelimit:

rotation.sizeLimit
==================

.. index::
   single: omfile; rotation.sizeLimit
   single: rotation.sizeLimit

.. summary-start

This permits to set a size limit on the output file.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omfile`.

:Name: rotation.sizeLimit
:Scope: action
:Type: size
:Default: action=0 (disabled)
:Required?: no
:Introduced: at least 5.x, possibly earlier

Description
-----------

This permits to set a size limit on the output file. When the limit is reached,
rotation of the file is tried. The rotation script needs to be configured via
`rotation.sizeLimitCommand`.

Please note that the size limit is not exact. Some excess bytes are permitted
to prevent messages from being split across two files. Also, a full batch of
messages is not terminated in between. As such, in practice, the size of the
output file can grow some KiB larger than configured.

Also avoid to configure a too-low limit, especially for busy files. Calling the
rotation script is relatively performance intense. As such, it could negatively
affect overall rsyslog performance.

Action usage
------------

.. _param-omfile-action-rotation-sizelimit:
.. _omfile.parameter.action.rotation-sizelimit:
.. code-block:: rsyslog

   action(type="omfile" rotation.sizeLimit="...")

Legacy names (for reference)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Historic names/directives for compatibility. Do not use in new configs.

.. _omfile.parameter.legacy.outchannel:

- $outchannel — maps to rotation.sizeLimit (status: legacy)

.. index::
   single: omfile; $outchannel
   single: $outchannel

See also
--------

See also :doc:`../../configuration/modules/omfile`.
