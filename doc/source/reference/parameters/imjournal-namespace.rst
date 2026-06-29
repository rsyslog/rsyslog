.. _param-imjournal-namespace:
.. _imjournal.parameter.module.namespace:

.. meta::
   :tag: module:imjournal
   :tag: parameter:Namespace

Namespace
=========

.. index::
   single: imjournal; Namespace
   single: Namespace

.. summary-start

Reads entries from a specific systemd journal namespace.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imjournal`.

:Name: Namespace
:Scope: module
:Type: string (see :doc:`../../rainerscript/constant_strings`)
:Default: module=unset
:Required?: no
:Introduced: 8.2608.0

Description
-----------
When set, ``imjournal`` opens the named systemd journal namespace instead of
the default system journal namespace. This is useful with services that use
systemd's ``LogNamespace=`` setting and should be processed by a dedicated
``imjournal`` configuration.

``Namespace`` uses the systemd ``sd_journal_open_namespace()`` API. If rsyslog
is built against a libsystemd version that does not provide that API, the
configuration fails with an explicit error.

This option is mutually exclusive with :ref:`param-imjournal-remote`.

Module usage
------------
.. _param-imjournal-module-namespace:
.. _imjournal.parameter.module.namespace-usage:
.. code-block:: rsyslog

   module(load="imjournal" Namespace="my-service")

See also
--------
See also :doc:`../../configuration/modules/imjournal`.
