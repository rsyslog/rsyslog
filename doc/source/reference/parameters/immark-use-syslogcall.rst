.. _param-immark-use-syslogcall:
.. _immark.parameter.module.use-syslogcall:

use.syslogCall
===============

.. index::
   single: immark; use.syslogCall
   single: use.syslogCall

.. summary-start

Chooses whether immark logs via syslog(3) or its internal pipeline.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/immark`.

:Name: use.syslogCall
:Scope: module
:Type: boolean
:Default: module=on
:Required?: no
:Introduced: 8.2012.0

Description
-----------
Controls how immark emits its periodic mark messages:

* ``on`` (default) — immark calls the system ``syslog(3)`` interface.
  Messages bypass rsyslog's internal queueing and are handled like any
  other syslog API submission.
* ``off`` — immark constructs the message internally and submits it to
  rsyslog's main queue. This enables features such as binding a custom
  :ref:`ruleset <param-immark-ruleset>` or applying mark-specific
  templates.

If a ruleset is configured while ``use.syslogCall`` remains ``on``,
immark issues a warning and forces this parameter to ``off`` so the
ruleset can take effect.

Module usage
------------
.. _immark.parameter.module.use-syslogcall-usage:

.. code-block:: rsyslog

   module(load="immark" use.syslogCall="off")

See also
--------

.. seealso::

   * :ref:`param-immark-ruleset`
   * :doc:`../../configuration/modules/immark`
