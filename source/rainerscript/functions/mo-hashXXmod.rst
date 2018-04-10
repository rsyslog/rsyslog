*********
HashXXmod
*********

Purpose
=======

Generates a number which is mod of given string's hash.

hash32mod(literal_string, modulo, <optional param, default = 0> seed)
---------------------------------------------------------------------

   Generates a number which is calculated on (32 bit hash of the given string % modulo)
    - If modulo is not a valid number, then 0 is returned.
    - If modulo is 0, then 0 is returned.
    - If seed is not a valid unsigned number, then 0 is returned.


hash64mod(literal_string, modulo, <optional param, default = 0> seed)
---------------------------------------------------------------------

   Generates a number which is calculated on (64 bit hash of the given string % modulo)
    - If modulo is not a valid number, then 0 is returned.
    - If modulo is 0, then 0 is returned.
    - If seed is not a valid unsigned number, then 0 is returned.


.. warning::
   Default hash implementation is non-crypto.


Example
=======

.. code-block:: none

  module(load="fmhash")

  if (hash64mod($!msg!request_id, 100) <= 30) then {
   //send out
  }


**Read more about it here** :doc:`Hash based sampling<../../tutorials/hash_sampling>`
