******
HashXX
******

Purpose
=======

Generates hash for a given string.

hash32(literal_string, <optional param, default = 0> seed)
----------------------------------------------------------

   Generates a 32 bit hash of the given string.
    - If seed is not a valid number, then 0 is returned.

hash64(literal_string, <optional param, default = 0> seed)
----------------------------------------------------------

  Generates a 64 bit hash of the given string.
   - If seed is not a valid number, then 0 is returned.

.. warning::
   Default hash implementation is non-crypto.


Example
=======

.. code-block:: none

  module(load="fmhash")

  set $.hash = hash64($!msg!field_1 & $!msg!field_2 & $!msg!field_3)
  set $!tag= $syslogtag & $.hash;
  //send out


**Read more about it here** :doc:`Hash based sampling<../../tutorials/hash_sampling>`
