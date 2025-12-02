.. _param-omlibdbi-pwd:
.. _omlibdbi.parameter.input.pwd:

PWD
===

.. index::
   single: omlibdbi; PWD
   single: PWD

.. summary-start

Sets the password for the database user defined via ``UID``.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omlibdbi`.

:Name: PWD
:Scope: input
:Type: word
:Default: input=none
:Required?: yes
:Introduced: Not documented

Description
-----------
Supply the password that matches the user provided through the ``UID``
parameter so omlibdbi can authenticate to the database.

Input usage
-----------
.. _param-omlibdbi-input-pwd-usage:
.. _omlibdbi.parameter.input.pwd-usage:

.. code-block:: rsyslog

   action(type="omlibdbi" driver="mysql" server="db.example.net"
          uid="dbwriter" pwd="sup3rSecret" db="syslog")

Legacy names (for reference)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Historic names/directives for compatibility. Do not use in new configs.

.. _omlibdbi.parameter.legacy.actionlibdbipassword:

- $ActionLibdbiPassword — maps to PWD (status: legacy)

.. index::
   single: omlibdbi; $ActionLibdbiPassword
   single: $ActionLibdbiPassword

See also
--------
See also :doc:`../../configuration/modules/omlibdbi`.
