.. _param-mmaitag-apikey:
.. _mmaitag.parameter.action.apikey:

apiKey
======

.. index::
   single: mmaitag; apiKey
   single: apiKey

.. summary-start

Sets the API key used to authenticate with the provider.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/mmaitag`.

:Name: apiKey
:Scope: action
:Type: string
:Default: none
:Required?: no (either ``apiKey`` or :ref:`param-mmaitag-apikey_file` must be set)
:Introduced: 9.0.0

Description
-----------
API key for the provider. Either this parameter or
:ref:`param-mmaitag-apikey_file` must be specified.

Action usage
-------------
.. _param-mmaitag-action-apikey:
.. _mmaitag.parameter.action.apikey-usage:

.. code-block:: rsyslog

   action(type="mmaitag" apiKey="ABC")

See also
--------
See also :doc:`../../configuration/modules/mmaitag`.
