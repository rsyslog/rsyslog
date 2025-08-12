.. _param-omfile-failonchownfailure:
.. _omfile.parameter.module.failonchownfailure:

failOnChOwnFailure
==================

.. index::
   single: omfile; failOnChOwnFailure
   single: failOnChOwnFailure

.. summary-start

This option modifies behaviour of file creation.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omfile`.

:Name: failOnChOwnFailure
:Scope: action
:Type: boolean
:Default: action=on
:Required?: no
:Introduced: at least 5.x, possibly earlier

Description
-----------

This option modifies behaviour of file creation. If different owners
or groups are specified for new files or directories and rsyslogd
fails to set these new owners or groups, it will log an error and NOT
write to the file in question if that option is set to "on". If it is
set to "off", the error will be ignored and processing continues.
Keep in mind, that the files in this case may be (in)accessible by
people who should not have permission. The default is "on".

Action usage
------------

.. _param-omfile-action-failonchownfailure:
.. _omfile.parameter.action.failonchownfailure:
.. code-block:: rsyslog

   action(type="omfile" failOnChOwnFailure="...")

Notes
-----

- Legacy documentation referred to the type as ``binary``; this maps to ``boolean``.

Legacy names (for reference)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Historic names/directives for compatibility. Do not use in new configs.

.. _omfile.parameter.legacy.failonchownfailure:

- $FailOnCHOwnFailure — maps to failOnChOwnFailure (status: legacy)

.. index::
   single: omfile; $FailOnCHOwnFailure
   single: $FailOnCHOwnFailure

See also
--------

See also :doc:`../../configuration/modules/omfile`.
