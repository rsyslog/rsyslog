.. _param-omfile-closetimeout:
.. _omfile.parameter.module.closetimeout:

closeTimeout
============

.. index::
   single: omfile; closeTimeout
   single: closeTimeout

.. summary-start

Specifies after how many minutes of inactivity a file is
automatically closed.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omfile`.

:Name: closeTimeout
:Scope: action
:Type: integer
:Default: action=File: 0 DynaFile: 10
:Required?: no
:Introduced: 8.3.3

Description
-----------

Specifies after how many minutes of inactivity a file is
automatically closed. Note that this functionality is implemented
based on the
:doc:`janitor process <../../concepts/janitor>`.
See its doc to understand why and how janitor-based times are
approximate.

Action usage
------------

.. _param-omfile-action-closetimeout:
.. _omfile.parameter.action.closetimeout:
.. code-block:: rsyslog

   action(type="omfile" closeTimeout="...")

See also
--------

See also :doc:`../../configuration/modules/omfile`.
