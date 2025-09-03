.. _param-mmkubernetes-cacheexpireinterval:
.. _mmkubernetes.parameter.action.cacheexpireinterval:

cacheexpireinterval
===================

.. index::
   single: mmkubernetes; cacheexpireinterval
   single: cacheexpireinterval

.. summary-start

Controls how often to check for expired metadata cache entries.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/mmkubernetes`.

:Name: cacheexpireinterval
:Scope: action
:Type: integer
:Default: action=-1
:Required?: no
:Introduced: at least 8.x, possibly earlier

Description
-----------
This parameter allows you to expire entries from the metadata cache.  The
values are:

- -1 (default) - disables metadata cache expiration
- 0 - check cache for expired entries before every cache lookup
- 1 or higher - the number is a number of seconds - check the cache
  for expired entries every this many seconds, when processing an
  entry

The cache is only checked if processing a record from Kubernetes.  There
isn't some sort of housekeeping thread that continually runs cleaning up
the cache.  When an record from Kubernetes is processed:

If `cacheexpireinterval` is -1, then do not check for cache expiration.
If `cacheexpireinterval` is 0, then check for cache expiration.
If `cacheexpireinterval` is greater than 0, check for cache expiration
if the last time we checked was more than this many seconds ago.

When cache expiration is checked, it will delete all cache entries which
have a ttl less than or equal to the current time.  The cache entry ttl
is set using the :ref:`param-mmkubernetes-cacheentryttl`.

Action usage
------------
.. _param-mmkubernetes-action-cacheexpireinterval:
.. _mmkubernetes.parameter.action.cacheexpireinterval-usage:

.. code-block:: rsyslog

   action(type="mmkubernetes" cacheExpireInterval="-1")

See also
--------
See also :doc:`../../configuration/modules/mmkubernetes`.
