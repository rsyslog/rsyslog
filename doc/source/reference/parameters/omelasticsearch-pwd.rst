.. _param-omelasticsearch-pwd:
.. _omelasticsearch.parameter.module.pwd:

pwd
===

.. index::
   single: omelasticsearch; pwd
   single: pwd

.. summary-start

Password for basic HTTP authentication.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omelasticsearch`.

:Name: pwd
:Scope: action
:Type: word
:Default: action=none
:Required?: no
:Introduced: at least 8.x, possibly earlier

Description
-----------
Supplies the password when Elasticsearch requires basic authentication.
This parameter cannot be combined with :ref:`param-omelasticsearch-apikey`.

Action usage
------------
.. _param-omelasticsearch-action-pwd:
.. _omelasticsearch.parameter.action.pwd:
.. code-block:: rsyslog

   action(type="omelasticsearch" pwd="...")

See also
--------
See also :doc:`../../configuration/modules/omelasticsearch`.
