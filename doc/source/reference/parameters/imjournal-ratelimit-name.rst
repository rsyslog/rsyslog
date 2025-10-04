.. _param-imjournal-ratelimit-name:

ratelimit.name
--------------

.. summary-start
.. include:: /reference/parameters/ratelimit-name-shared.rst
.. summary-end


When ``ratelimit.name`` is set for *imjournal*, the module uses the
interval and burst defined by the referenced object and the inline
``ratelimit.interval``/``ratelimit.burst`` settings must not be present.
