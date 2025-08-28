.. _param-omfile-sig-provider:
.. _omfile.parameter.module.sig-provider:

sig.Provider
============

.. index::
   single: omfile; sig.Provider
   single: sig.Provider

.. summary-start

Selects a signature provider for log signing.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omfile`.

:Name: sig.Provider
:Scope: action
:Type: word
:Default: action=no signature provider
:Required?: no
:Introduced: at least 8.x, possibly earlier

Description
-----------

Selects a signature provider for log signing. By selecting a provider,
the signature feature is turned on.

Currently there is one signature provider available: ":doc:`ksi_ls12 <../../configuration/modules/sigprov_ksi12>`".

Previous signature providers ":doc:`gt <../../configuration/modules/sigprov_gt>`" and
":doc:`ksi <../../configuration/modules/sigprov_ksi>`" are deprecated.

Action usage
------------

.. _param-omfile-action-sig-provider:
.. _omfile.parameter.action.sig-provider:
.. code-block:: rsyslog

   action(type="omfile" sig.Provider="...")

See also
--------

See also :doc:`../../configuration/modules/omfile`.
