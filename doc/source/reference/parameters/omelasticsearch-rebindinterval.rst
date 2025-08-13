.. _param-omelasticsearch-rebindinterval:
.. _omelasticsearch.parameter.module.rebindinterval:

rebindinterval
==============

.. index::
   single: omelasticsearch; rebindinterval
   single: rebindinterval

.. summary-start

Operations after which to reconnect; `-1` disables periodic reconnect.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omelasticsearch`.

:Name: rebindinterval
:Scope: action
:Type: integer
:Default: action=-1
:Required?: no
:Introduced: at least 8.x, possibly earlier

Description
-----------
After this many operations the module drops and re-establishes the connection. Useful behind load balancers.

Action usage
------------
.. _param-omelasticsearch-action-rebindinterval:
.. _omelasticsearch.parameter.action.rebindinterval:
.. code-block:: rsyslog

   action(type="omelasticsearch" rebindinterval="...")

See also
--------
See also :doc:`../../configuration/modules/omelasticsearch`.
