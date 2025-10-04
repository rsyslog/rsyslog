.. _param-imklog-ratelimit-name:

ratelimit.name
--------------

.. summary-start
.. include:: /reference/parameters/ratelimit-name-shared.rst
.. summary-end


``imklog`` applies the shared configuration to its kernel log ratelimiter.
Inline ``ratelimit.interval``/``ratelimit.burst`` parameters must be omitted
when a name is provided.
