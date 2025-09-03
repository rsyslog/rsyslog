.. _param-mmkubernetes-de-dot-separator:
.. _mmkubernetes.parameter.action.de-dot-separator:

de_dot_separator
================

.. index::
   single: mmkubernetes; de_dot_separator
   single: de_dot_separator

.. summary-start

Defines the string used to replace dots when ``de_dot`` is enabled.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/mmkubernetes`.

:Name: de_dot_separator
:Scope: action
:Type: word
:Default: _
:Required?: no
:Introduced: at least 8.x, possibly earlier

Description
-----------
When processing labels and annotations, if the :ref:`param-mmkubernetes-de-dot` parameter is
set to `"on"`, the key strings will have their `.` characters replaced
with the string specified by the string value of this parameter.

Action usage
------------
.. _param-mmkubernetes-action-de-dot-separator:
.. _mmkubernetes.parameter.action.de-dot-separator-usage:

.. code-block:: rsyslog

   action(type="mmkubernetes" deDotSeparator="_")

See also
--------
See also :doc:`../../configuration/modules/mmkubernetes`.
