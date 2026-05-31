.. _param-omrelp-ratelimit-name:

RateLimit.Name
^^^^^^^^^^^^^^

.. summary-start

Binds the omrelp action to a named ``ratelimit()`` object.
.. summary-end

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "string", "", "no", "none"

Assigns a named :doc:`ratelimit object <../../rainerscript/configuration_objects/ratelimit>` to this
omrelp action. ``ratelimit.name`` is mutually exclusive with
``ratelimit.interval`` and ``ratelimit.burst``; if both forms are configured,
the named ratelimit object takes precedence.
