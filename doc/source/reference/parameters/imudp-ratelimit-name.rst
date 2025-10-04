.. _param-imudp-ratelimit-name:

ratelimit.name
--------------

.. summary-start
.. include:: /reference/parameters/ratelimit-name-shared.rst
.. summary-end


``imudp`` uses the shared configuration for all sockets created by the
listener. Inline ``ratelimit.interval``/``ratelimit.burst`` parameters must
be omitted when a name is provided.
