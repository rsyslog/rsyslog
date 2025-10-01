.. _param-imhttp-ratelimit-name:

ratelimit.name
--------------

.. summary-start
.. include:: /reference/parameters/ratelimit-name-shared.rst
.. summary-end


``imhttp`` applies the named ratelimit to all HTTP listener threads.
Inline ``ratelimit.interval``/``ratelimit.burst`` parameters must not be
specified when this parameter is used.
