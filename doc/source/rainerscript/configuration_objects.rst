configuration objects
=====================

.. note::

  Configuration object parameters are case-insensitive.

Common Parameters
-----------------

config.enabled
^^^^^^^^^^^^^^

.. versionadded:: 8.33.0

All configuration objects have a ``config.enabled`` parameter.
For auto-generated configs, it is useful to have the ability to disable some
config constructs even though they may be specified inside the config. This
can be done via the ``config.enabled`` parameter.
If set to ``on`` or not specified, the construct will be
used, if set to any other value, it will be ignored.
This can be used together with the backtick functionality to enable or
disable parts of the configuration from either a file or environment variable.

Example:

Let's say we want to conditionally load a module. Environment variable
``LOAD_IMPTCP`` will be either unset or ``off`` .
Then we can use this config construct:

.. code-block:: none
   :emphasize-lines: 2

    module(load="imptcp"
        config.enabled=`echo $LOAD_IMPTCP`)

If the variable is set to ``off``, the module will **not** be loaded.

Objects
-------

.. toctree::
   :maxdepth: 1
   :hidden:

   configuration_objects/ratelimit


action()
^^^^^^^^

The :doc:`action <../configuration/actions>`  object is the primary means of
describing actions to be carried out.

global()
^^^^^^^^

This is used to set global configuration parameters. For details, please
see the :doc:`rsyslog global configuration object <global>`.

input()
^^^^^^^

The :doc:`input <../configuration/input>` object is the primary means of
describing inputs, which are used to gather messages for rsyslog processing.

module()
^^^^^^^^

The module object is used to load plugins.

parser()
^^^^^^^^

The :doc:`parser <../configuration/parser>` object is used to define
custom parser objects.

timezone()
^^^^^^^^^^

The :doc:`timezone <../configuration/timezone>` object is used to define
timezone settings.

include()
^^^^^^^^^

The :doc:`include <include>`  object is use to include configuration snippets
stored elsewhere into the configuration.

ratelimit()
^^^^^^^^^^^

The :doc:`ratelimit <configuration_objects/ratelimit>` object is used to define named rate limit policies.
