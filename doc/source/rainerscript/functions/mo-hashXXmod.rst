.. SPDX-License-Identifier: Apache-2.0

.. meta::
   :description: Generate a hash modulo a supplied number with an optional seed.
   :keywords: rsyslog, RainerScript, fmhash, hash, seed

*********
HashXXmod
*********

Purpose
=======

.. summary-start

Generates a number which is mod of given string's hash.

.. summary-end

hash32mod(literal_string, modulo) / hash32mod(literal_string, modulo, seed)
---------------------------------------------------------------------------

   Generates a number which is calculated on (32 bit hash of the given string % modulo)
    - If modulo is not a valid number, then 0 is returned.
    - If modulo is 0, then 0 is returned.
    - Seed is an optional parameter with default = 0.
    - If seed cannot be converted to a number, then 0 is returned.


hash64mod(literal_string, modulo) / hash64mod(literal_string, modulo, seed)
---------------------------------------------------------------------------

   Generates a number which is calculated on (64 bit hash of the given string % modulo)
    - If modulo is not a valid number, then 0 is returned.
    - If modulo is 0, then 0 is returned.
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

  if (hash64mod($!msg!request_id, 100) <= 30) then {
   //send out
  }

.. seealso::
  :doc:`Hash based sampling<../../tutorials/hash_sampling>`
