.. _param-omprog-binary:
.. _omprog.parameter.action.binary:

binary
======

.. index::
   single: omprog; binary
   single: binary

.. summary-start

Specifies the command line of the external program to execute.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omprog`.

:Name: binary
:Scope: action
:Type: string
:Default: none
:Required?: yes
:Introduced: at least 5.x, possibly earlier

Description
-----------
Full path and command line parameters of the external program to execute.
Arbitrary external programs should be placed under the /usr/libexec/rsyslog directory.
That is, the binaries put in this namespaced directory are meant for the consumption
of rsyslog, and are not intended to be executed by users.
In legacy config, it is **not possible** to specify command line parameters.

Action usage
------------
.. _param-omprog-action-binary:
.. _omprog.parameter.action.binary-usage:

.. code-block:: rsyslog

   action(type="omprog" binary="/usr/libexec/rsyslog/log.sh")

Legacy names (for reference)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Historic names/directives for compatibility. Do not use in new configs.

.. _omprog.parameter.legacy.actionomprogbinary:

- $ActionOMProgBinary — maps to binary (status: legacy)

.. index::
   single: omprog; $ActionOMProgBinary
   single: $ActionOMProgBinary

See also
--------
See also :doc:`../../configuration/modules/omprog`.
