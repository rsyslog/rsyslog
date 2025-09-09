.. _param-mmrfc5424addhmac-key:
.. _mmrfc5424addhmac.parameter.action.key:

key
===

.. index::
   single: mmrfc5424addhmac; key
   single: key

.. summary-start

Specifies the key used to generate the HMAC.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/mmrfc5424addhmac`.

:Name: key
:Scope: action
:Type: string
:Default: none
:Required?: yes
:Introduced: 7.5.6

Description
-----------
The key to be used to generate the HMAC.

Action usage
------------
.. _param-mmrfc5424addhmac-action-key:
.. _mmrfc5424addhmac.parameter.action.key-usage:

.. code-block:: rsyslog

   action(type="mmrfc5424addhmac" key="a-long-and-very-secret-key-phrase")

See also
--------
See also :doc:`../../configuration/modules/mmrfc5424addhmac`.

