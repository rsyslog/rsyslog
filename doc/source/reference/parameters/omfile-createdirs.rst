.. _param-omfile-createdirs:
.. _omfile.parameter.module.createdirs:

createDirs
==========

.. index::
   single: omfile; createDirs
   single: createDirs

.. summary-start

Create directories on an as-needed basis.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omfile`.

:Name: createDirs
:Scope: action
:Type: boolean
:Default: action=on
:Required?: no
:Introduced: at least 5.x, possibly earlier

Description
-----------

Create directories on an as-needed basis

Action usage
------------

.. _param-omfile-action-createdirs:
.. _omfile.parameter.action.createdirs:
.. code-block:: rsyslog

   action(type="omfile" createDirs="...")

Notes
-----

- Legacy documentation referred to the type as ``binary``; this maps to ``boolean``.

Legacy names (for reference)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Historic names/directives for compatibility. Do not use in new configs.

.. _omfile.parameter.legacy.createdirs:

- $CreateDirs — maps to createDirs (status: legacy)

.. index::
   single: omfile; $CreateDirs
   single: $CreateDirs

See also
--------

See also :doc:`../../configuration/modules/omfile`.
