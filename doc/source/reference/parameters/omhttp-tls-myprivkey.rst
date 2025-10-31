.. _param-omhttp-tls-myprivkey:
.. _omhttp.parameter.module.tls-myprivkey:

tls.myprivkey
=============

.. index::
   single: omhttp; tls.myprivkey
   single: tls.myprivkey

.. summary-start

Provides the private key file that pairs with :ref:`param-omhttp-tls-mycert`.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omhttp`.

:Name: tls.myprivkey
:Scope: module
:Type: word
:Default: module=none
:Required?: no
:Introduced: Not specified

Description
-----------
This parameter sets the path to the SSL private key. Expects ``.pem`` format.

Module usage
------------
.. _param-omhttp-module-tls-myprivkey:
.. _omhttp.parameter.module.tls-myprivkey-usage:

.. code-block:: rsyslog

   module(load="omhttp")

   action(
       type="omhttp"
       useHttps="on"
       tls.myPrivKey="/etc/rsyslog/certs/omhttp-client.key"
   )

See also
--------
See also :doc:`../../configuration/modules/omhttp`.
