***********
substring()
***********

Purpose
=======

`substring(str, start, subStringLength)`

Creates a substring from `str`. The substring begins at `start` and is
at most `subStringLength` characters long. If `start` is higher than the
length of the string, the result will always be an empty string. The
same is the case if `subStringLength` is zero.

.. note::

   The first character of the string has the value 0. So if you set
   start to '2', the substring will start with the third character.

If you want to drop only some characters at the beginning of the string,
select an overlay large `subStringLength`, e.g. 1000000. As the length is
then always larger as any normal string, the function will return all but
the first part of the string.

In order to aid removing `n` characters from the end of the string, you
can specify a **negative** `subStringLength`. This will result in an
actual `subStringLength` that is the string-length minus the specified
`subStringLength`. This is actually a shortcut for

.. code-block:: none

    subStringLength = strlen(str) - n

Specifying a negative `subStringLength` does not only offer a more
compact expression, it also has some performance benefits over the
use of `strlen()`.


Examples
========

In the following example a substring from the string is created,
starting with the 4th character and being 5 characters long.

.. code-block:: none

   substring("123456789", 3, 5)


This example will result in the substring "45678".

In the following example the first and the last character
of the string will be removed.

.. code-block:: none

   substring("123456789", 1, -2)


This example will result in the substring "2345678".

Note the `-2` value for `subStringLength`. As the extraction skips the
first character, you only need a substring of `strlen("123456789") - 2`
characters, as you want to remove the last character as well. In other
words: if you want to remove the first **and** last character from the
string, the substring in question is **two** characters shorter than the
original string! We know this is a bit counter-intuitive, this we
wanted to make you aware.

As another example let us assume you want to drop the first two and the
last character. In this case you would use `subStringLength` of three:

.. code-block:: none

   substring("123456789", 2, -3)
