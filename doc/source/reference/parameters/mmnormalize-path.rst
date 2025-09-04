.. _param-mmnormalize-path:
.. _mmnormalize.parameter.action.path:

path
====

.. index::
   single: mmnormalize; path
   single: path

.. summary-start

Sets the JSON path where parsed elements are stored.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/mmnormalize`.

:Name: path
:Scope: action
:Type: word
:Default: action=$!
:Required?: no
:Introduced: at least 6.1.2, possibly earlier

Description
-----------
Specifies the JSON path under which parsed elements should be placed. By
default, all parsed properties are merged into root of message properties. You
can place them under a subtree, instead. You can place them in local
variables, also, by setting ``path="$."``.

Action usage
-------------
.. _param-mmnormalize-action-path:
.. _mmnormalize.parameter.action.path-usage:

.. code-block:: rsyslog

   action(type="mmnormalize" path="$!subtree")

See also
--------
See also :doc:`../../configuration/modules/mmnormalize`.
