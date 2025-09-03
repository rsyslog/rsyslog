.. _param-mmkubernetes-filenamerules:
.. _mmkubernetes.parameter.action.filenamerules:

filenamerules
=============

.. index::
   single: mmkubernetes; filenamerules
   single: filenamerules

.. summary-start

Defines lognorm rules to parse json-file log filenames for metadata.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/mmkubernetes`.

:Name: filenamerules
:Scope: action
:Type: word
:Default: SEE BELOW
:Required?: no
:Introduced: at least 8.x, possibly earlier

Description
-----------
.. note::

    This directive is not supported with liblognorm 2.0.2 and earlier.

When processing json-file logs, these are the lognorm rules to use to
match the filename and extract metadata.  The default value is::

    rule=:/var/log/containers/%pod_name:char-to:_%_%namespace_name:char-to:_%_%contai\
    ner_name_and_id:char-to:.%.log

.. note::

    In the above rules, the slashes ``\`` ending each line indicate
    line wrapping - they are not part of the rule.

Action usage
------------
.. _param-mmkubernetes-action-filenamerules:
.. _mmkubernetes.parameter.action.filenamerules-usage:

.. code-block:: rsyslog

   action(type="mmkubernetes" filenameRules="...")

See also
--------
See also :doc:`../../configuration/modules/mmkubernetes`.
