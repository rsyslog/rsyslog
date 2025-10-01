.. _param-imtcp-ratelimit-name:

ratelimit.name
--------------

.. summary-start
.. include:: /reference/parameters/ratelimit-name-shared.rst
.. summary-end


When using TLS listeners, the named ratelimit is shared by all sockets in
this ``imtcp`` instance. Inline ``ratelimit.interval`` and
``ratelimit.burst`` options must be omitted when specifying a name.
