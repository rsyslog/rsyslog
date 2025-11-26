.. _param-omelasticsearch-usehttps:
.. _omelasticsearch.parameter.module.usehttps:

usehttps
========

.. index::
   single: omelasticsearch; usehttps
   single: usehttps

.. summary-start

Default scheme for servers missing one.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omelasticsearch`.

:Name: usehttps
:Scope: action
:Type: boolean
:Default: action=off
:Required?: no
:Introduced: at least 8.x, possibly earlier

Description
-----------
If enabled, entries in `server` without an explicit scheme use HTTPS; otherwise HTTP is assumed. Servers that already specify ``http`` or ``https`` keep their declared scheme.

Action usage
------------
.. _param-omelasticsearch-action-usehttps:
.. _omelasticsearch.parameter.action.usehttps:
.. code-block:: rsyslog

   action(type="omelasticsearch" usehttps="...")

Notes
-----
- Previously documented as a "binary" option.

See also
--------
See also :doc:`../../configuration/modules/omelasticsearch`.
