.. _param-omprog-filecreatemode:
.. _omprog.parameter.action.filecreatemode:

fileCreateMode
==============

.. index::
   single: omprog; fileCreateMode
   single: fileCreateMode

.. summary-start

Sets permissions for the output file when it is created.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omprog`.

:Name: fileCreateMode
:Scope: action
:Type: string
:Default: action=0600
:Required?: no
:Introduced: v8.38.0

Description
-----------
Permissions the :ref:`param-omprog-output` file will be created with, in case the file does not
exist. The value must be a 4-digit octal number, with the initial digit being
zero. Please note that the actual permission depends on the rsyslogd process
umask. If in doubt, use ``$umask 0000`` right at the beginning of the
configuration file to remove any restrictions.

Action usage
------------
.. _param-omprog-action-filecreatemode:
.. _omprog.parameter.action.filecreatemode-usage:

.. code-block:: rsyslog

   action(type="omprog"
          output="/var/log/prog.log"
          fileCreateMode="0640")

See also
--------
See also :doc:`../../configuration/modules/omprog`.
