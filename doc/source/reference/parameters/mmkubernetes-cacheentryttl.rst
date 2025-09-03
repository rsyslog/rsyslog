.. _param-mmkubernetes-cacheentryttl:
.. _mmkubernetes.parameter.action.cacheentryttl:

cacheentryttl
=============

.. index::
   single: mmkubernetes; cacheentryttl
   single: cacheentryttl

.. summary-start

Sets the maximum age of entries in the metadata cache.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/mmkubernetes`.

:Name: cacheentryttl
:Scope: action
:Type: integer
:Default: 3600
:Required?: no
:Introduced: at least 8.x, possibly earlier

Description
-----------
This parameter allows you to set the maximum age (time-to-live, or ttl) of
an entry in the metadata cache.  The value is in seconds.  The default value
is `3600` (one hour).  When cache expiration is checked, if a cache entry
has a ttl less than or equal to the current time, it will be removed from
the cache.

This option is only used if :ref:`param-mmkubernetes-cacheexpireinterval` is 0 or greater.

This value must be 0 or greater, otherwise, if :ref:`param-mmkubernetes-cacheexpireinterval` is 0
or greater, you will get an error.

Action usage
------------
.. _param-mmkubernetes-action-cacheentryttl:
.. _mmkubernetes.parameter.action.cacheentryttl-usage:

.. code-block:: rsyslog

   action(type="mmkubernetes" cacheEntryTtl="3600")

See also
--------
See also :doc:`../../configuration/modules/mmkubernetes`.
