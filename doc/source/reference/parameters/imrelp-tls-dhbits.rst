.. _param-imrelp-tls-dhbits:
.. _imrelp.parameter.input.tls-dhbits:

TLS.dhbits
==========

.. index::
   single: imrelp; TLS.dhbits
   single: TLS.dhbits

.. summary-start

Specifies the Diffie-Hellman key size, overriding the librelp default when set.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imrelp`.

:Name: TLS.dhbits
:Scope: input
:Type: integer
:Default: input=0
:Required?: no
:Introduced: Not documented

Description
-----------
This setting controls how many bits are used for Diffie-Hellman key generation.
If not set, the librelp default is used. For security reasons, at least 1024
bits should be used. Please note that the number of bits must be supported by
GnuTLS. If an invalid number is given, rsyslog will report an error when the
listener is started. We do this to be transparent to changes/upgrades in GnuTLS
(to check at config processing time, we would need to hardcode the supported bits
and keep them in sync with GnuTLS - this is even impossible when custom GnuTLS
changes are made...).

Input usage
-----------
.. _param-imrelp-input-tls-dhbits:
.. _imrelp.parameter.input.tls-dhbits-usage:

.. code-block:: rsyslog

   input(type="imrelp" port="2514" tls="on" tls.dhBits="2048")

See also
--------
See also :doc:`../../configuration/modules/imrelp`.
