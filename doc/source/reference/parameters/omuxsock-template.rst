.. _param-omuxsock-template:
.. _omuxsock.parameter.module.template:
.. _omuxsock.parameter.action.template:

template
========

.. index::
   single: omuxsock; template
   single: template

.. summary-start

Sets the template for formatting messages sent to Unix sockets.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omuxsock`.

:Name: template
:Scope: module, action
:Type: word
:Default: RSYSLOG_TraditionalForwardFormat
:Required?: no
:Introduced: at least 5.x, possibly earlier

Description
-----------
Name of the :doc:`template <../../configuration/templates>` to use to format log messages
sent to Unix sockets. When set at module scope, it becomes the default for actions
without an explicitly configured template. When set on an action, it overrides the
module-level default for that action.

Module usage
------------
.. _param-omuxsock-module-template:
.. _omuxsock.parameter.module.template-usage:

.. code-block:: rsyslog

   module(load="omuxsock" template="RSYSLOG_TraditionalForwardFormat")

Action usage
------------
.. _param-omuxsock-action-template:
.. _omuxsock.parameter.action.template-usage:

.. code-block:: rsyslog

   action(type="omuxsock" template="RSYSLOG_TraditionalForwardFormat")

See also
--------
See also :doc:`../../configuration/modules/omuxsock`.
