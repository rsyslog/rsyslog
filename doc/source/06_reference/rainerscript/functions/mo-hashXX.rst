******
HashXX
******

Purpose
=======

Generates hash for a given string.

hash32(literal_string) / hash32(literal_string, seed)
-----------------------------------------------------

   Generates a 32 bit hash of the given string.
    - Seed is an optional parameter with default = 0.
    - If seed is not a valid number, then 0 is returned.

hash64(literal_string) / hash64(literal_string, seed)
-----------------------------------------------------

  Generates a 64 bit hash of the given string.
   - Seed is an optional parameter with default = 0.
   - If seed is not a valid number, then 0 is returned.

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
  :doc:`hash sampling tutorial <../../../07_tutorials/hash_sampling>`.
