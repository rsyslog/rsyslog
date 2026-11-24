.. SPDX-License-Identifier: Apache-2.0

.. meta::
   :description: Generate 32-bit or 64-bit hashes with an optional seed.
   :keywords: rsyslog, RainerScript, fmhash, hash, seed

******
HashXX
******

Purpose
=======

.. summary-start

Generates hash for a given string.

.. summary-end

hash32(literal_string) / hash32(literal_string, seed)
-----------------------------------------------------

   Generates a 32 bit hash of the given string.
    - Seed is an optional parameter with default = 0.
    - If seed cannot be converted to a number, then 0 is returned.

hash64(literal_string) / hash64(literal_string, seed)
-----------------------------------------------------

  Generates a 64 bit hash of the given string.
   - Seed is an optional parameter with default = 0.
   - If seed cannot be converted to a number, then 0 is returned.

Seed conversion
===============

After RainerScript converts the seed to a number, that number is converted to
an unsigned 32-bit integer before hashing. For values representable as
RainerScript numbers, negative seeds and seeds greater than 4294967295 wrap
modulo 4294967296. For example,
``-1`` and ``4294967295`` select the same seed, and ``4294967296`` selects
seed ``0``. Decimal numeric strings are accepted with the same conversion.


.. warning::

   - Default hash implementation is non-crypto.
   - To use xxhash enable compile time flag.


Example
=======

.. code-block:: none

  module(load="fmhash")

  set $.hash = hash64($!msg!field_1 & $!msg!field_2 & $!msg!field_3)
  set $!tag= $syslogtag & $.hash;
  //send out

.. seealso::
  :doc:`Hash based sampling<../../tutorials/hash_sampling>`
