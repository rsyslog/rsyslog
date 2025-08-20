.. _param-omprog-template:
.. _omprog.parameter.action.template:

template
========

.. index::
   single: omprog; template
   single: template

.. summary-start

Sets the template for formatting messages sent to the program.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omprog`.

:Name: template
:Scope: action
:Type: word
:Default: action=RSYSLOG_FileFormat
:Required?: no
:Introduced: at least 5.x, possibly earlier

Description
-----------
Name of the :doc:`template <../../configuration/templates>` to use to format the log messages
passed to the external program.

Action usage
------------
.. _param-omprog-action-template:
.. _omprog.parameter.action.template-usage:

.. code-block:: rsyslog

   action(type="omprog" template="RSYSLOG_FileFormat")

See also
--------
See also :doc:`../../configuration/modules/omprog`.
