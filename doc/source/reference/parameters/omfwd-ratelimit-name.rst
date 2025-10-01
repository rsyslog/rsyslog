.. _param-omfwd-ratelimit-name:

ratelimit.name
--------------

.. summary-start
.. include:: /reference/parameters/ratelimit-name-shared.rst
.. summary-end


When set, the shared configuration controls throttling for the retry logic
inside ``omfwd``. Inline ``ratelimit.interval``/``ratelimit.burst`` options
must be omitted.
