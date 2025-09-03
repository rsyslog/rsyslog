.. _param-mmkubernetes-dstmetadatapath:
.. _mmkubernetes.parameter.action.dstmetadatapath:

dstmetadatapath
===============

.. index::
   single: mmkubernetes; dstmetadatapath
   single: dstmetadatapath

.. summary-start

Defines where the ``kubernetes`` and ``docker`` properties are written.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/mmkubernetes`.

:Name: dstmetadatapath
:Scope: action
:Type: word
:Default: action=$!
:Required?: no
:Introduced: at least 8.x, possibly earlier

Description
-----------
This is the where the `kubernetes` and `docker` properties will be
written.  By default, the module will add `$!kubernetes` and
`$!docker`.

Action usage
------------
.. _param-mmkubernetes-action-dstmetadatapath:
.. _mmkubernetes.parameter.action.dstmetadatapath-usage:

.. code-block:: rsyslog

   action(type="mmkubernetes" dstMetadataPath="$!")

See also
--------
See also :doc:`../../configuration/modules/mmkubernetes`.
