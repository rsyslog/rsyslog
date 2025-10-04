.. _param-imuxsock-ratelimit-name:

ratelimit.name
--------------

.. summary-start
.. include:: /reference/parameters/ratelimit-name-shared.rst
.. summary-end


Each socket created by ``imuxsock`` shares the referenced configuration.
Inline ``ratelimit.interval``/``ratelimit.burst``/``ratelimit.severity``
settings must be omitted when a name is given.
