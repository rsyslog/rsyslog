.. _param-mmaitag-apikey_file:
.. _mmaitag.parameter.action.apikey_file:

apiKeyFile
==========

.. index::
   single: mmaitag; apiKeyFile
   single: apiKeyFile

.. summary-start

Specifies a file containing the API key for the provider.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/mmaitag`.

:Name: apiKeyFile
:Scope: action
:Type: string
:Default: none
:Required?: no (either :ref:`param-mmaitag-apikey` or ``apiKeyFile`` must be set)
:Introduced: 9.0.0

Description
-----------
File containing the API key for the provider. If :ref:`param-mmaitag-apikey`
is not set, the module reads the first line of this file and uses it as the
API key.

Action usage
-------------
.. _param-mmaitag-action-apikey_file:
.. _mmaitag.parameter.action.apikey_file-usage:

.. code-block:: rsyslog

   action(type="mmaitag" apiKeyFile="/path/to/keyfile")

See also
--------
See also :doc:`../../configuration/modules/mmaitag`.
