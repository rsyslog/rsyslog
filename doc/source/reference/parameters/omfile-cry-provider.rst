.. _param-omfile-cry-provider:
.. _omfile.parameter.module.cry-provider:

cry.Provider
============

.. index::
   single: omfile; cry.Provider
   single: cry.Provider

.. summary-start

Selects a crypto provider for log encryption.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omfile`.

:Name: cry.Provider
:Scope: action
:Type: word
:Default: action=no crypto provider
:Required?: no
:Introduced: at least 8.x, possibly earlier

Description
-----------

Selects a crypto provider for log encryption. By selecting a provider,
the encryption feature is turned on.

Currently, there are two providers called ":doc:`gcry <../../configuration/cryprov_gcry>`" and
":doc:`ossl <../../configuration/cryprov_ossl>`".

Action usage
------------

.. _param-omfile-action-cry-provider:
.. _omfile.parameter.action.cry-provider:
.. code-block:: rsyslog

   action(type="omfile" cry.Provider="...")

See also
--------

See also :doc:`../../configuration/modules/omfile`.
