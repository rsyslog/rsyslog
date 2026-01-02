.. _param-imtcp-persourcekeytpl:
.. _imtcp.parameter.input.persourcekeytpl:

perSourceKeyTpl
===============

.. index::
   single: imtcp; perSourceKeyTpl

.. summary-start

Selects the template used to compute the per-source key.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imtcp`.

:Name: perSourceKeyTpl
:Scope: input
:Type: string
:Default: RSYSLOG_PerSourceKey (``%hostname%``)
:Required?: no
:Introduced: 8.2602.0

Description
-----------
Controls how the sender key is derived for per-source rate limiting. The
template is evaluated per message and the resulting string is used to look
up per-source limits and overrides. The default template is equivalent to
``%hostname%``.

Input usage
-----------
.. _param-imtcp-input-persourcekeytpl:

.. code-block:: rsyslog

   template(name="PerSourceKey" type="string" string="%hostname%")
   input(type="imtcp" perSourceRate="on" perSourceKeyTpl="PerSourceKey")

See also
--------
See also :doc:`../../configuration/modules/imtcp`.
