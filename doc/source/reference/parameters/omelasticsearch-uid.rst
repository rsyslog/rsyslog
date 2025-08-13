.. _param-omelasticsearch-uid:
.. _omelasticsearch.parameter.module.uid:

uid
===

.. index::
   single: omelasticsearch; uid
   single: uid

.. summary-start

User name for basic HTTP authentication.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omelasticsearch`.

:Name: uid
:Scope: action
:Type: word
:Default: action=none
:Required?: no
:Introduced: at least 8.x, possibly earlier

Description
-----------
Supplies the user name when Elasticsearch requires basic authentication.

Action usage
------------
.. _param-omelasticsearch-action-uid:
.. _omelasticsearch.parameter.action.uid:
.. code-block:: rsyslog

   action(type="omelasticsearch" uid="...")

See also
--------
See also :doc:`../../configuration/modules/omelasticsearch`.
