.. _param-imjournal-remote:
.. _imjournal.parameter.module.remote:

.. meta::
   :tag: module:imjournal
   :tag: parameter:Remote

Remote
======

.. index::
   single: imjournal; Remote
   single: Remote

.. summary-start

Also read journal files from remote sources.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imjournal`.

:Name: Remote
:Scope: module
:Type: boolean
:Default: module=off
:Required?: no
:Introduced: 8.1910.0

Description
-----------
When enabled, the module processes not only local journal files but also those
originating from remote systems stored on the host.

Module usage
------------
.. _param-imjournal-module-remote:
.. _imjournal.parameter.module.remote-usage:
.. code-block:: rsyslog

   module(load="imjournal" Remote="...")

Notes
-----
- Historic documentation called this a ``binary`` option; it is boolean.

See also
--------
See also :doc:`../../configuration/modules/imjournal`.
