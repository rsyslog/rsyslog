.. _param-imklog-ratelimitburst:
.. _imklog.parameter.module.ratelimitburst:

RatelimitBurst
==============

.. index::
   single: imklog; RatelimitBurst
   single: RatelimitBurst

.. summary-start

Specifies how many messages imklog can emit within the configured rate-limiting interval.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imklog`.

:Name: RatelimitBurst
:Scope: module
:Type: integer
:Default: 10000
:Required?: no
:Introduced: 8.35.0

Description
-----------
Specifies the rate-limiting burst in number of messages. Set it high to
preserve all boot-up messages.

Module usage
------------
.. _param-imklog-module-ratelimitburst:
.. _imklog.parameter.module.ratelimitburst-usage:

.. code-block:: rsyslog

   module(load="imklog" ratelimitBurst="20000")

See also
--------
See also :doc:`../../configuration/modules/imklog`.
