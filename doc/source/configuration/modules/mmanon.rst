****************************************
IP Address Anonymization Module (mmanon)
****************************************

===========================  ===========================================================================
**Module Name:**             **mmanon**
**Author:**                  `Rainer Gerhards <https://rainer.gerhards.net/>`_ <rgerhards@adiscon.com>
**Available since:**         7.3.7
===========================  ===========================================================================


Purpose
=======

The mmanon module permits to anonymize IP addresses. It is a message
modification module that actually changes the IP address inside the
message, so after calling mmanon, the original message can no longer be
obtained. Note that anonymization will break digital signatures on the
message, if they exist.

Please note that log files can also be anonymized via
`SLFA <https://jan.gerhards.net/2017/10/12/slfa-release/>`_ after they
have been created.

*How are IP-Addresses defined?*

We assume that an IPv4 address consists of four octets in dotted notation,
where each of the octets has a value between 0 and 255, inclusively.

An IPv6 is defined by being between zero and eight hex values between 0
and ffff. These are separated by ':'. Leading zeros in blocks can be omitted
and blocks full of zeros can be abbreviated by using '::'. However, this
can only happen once in an IP address.

An IPv6 address with embedded IPv4 is an IPv6 address where the last two blocks
have been replaced by an IPv4 address. (see also: RFC4291, 2.2.3) 


Configuration Parameters
========================

.. note::

   Parameter names are case-insensitive; camelCase is recommended for readability.


Action Parameters
-----------------

Parameters starting with 'IPv4.' will configure IPv4 anonymization,
while 'IPv6.' parameters do the same for IPv6 anonymization.

.. list-table::
   :widths: 30 70
   :header-rows: 1

   * - Parameter
     - Summary
   * - :ref:`ipv4.enable <param-mmanon-ipv4-enable>`
     - .. include:: ../../reference/parameters/mmanon-ipv4-enable.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`ipv4.mode <param-mmanon-ipv4-mode>`
     - .. include:: ../../reference/parameters/mmanon-ipv4-mode.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`ipv4.bits <param-mmanon-ipv4-bits>`
     - .. include:: ../../reference/parameters/mmanon-ipv4-bits.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`ipv4.replaceChar <param-mmanon-ipv4-replacechar>`
     - .. include:: ../../reference/parameters/mmanon-ipv4-replacechar.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`ipv6.enable <param-mmanon-ipv6-enable>`
     - .. include:: ../../reference/parameters/mmanon-ipv6-enable.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`ipv6.anonMode <param-mmanon-ipv6-anonmode>`
     - .. include:: ../../reference/parameters/mmanon-ipv6-anonmode.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`ipv6.bits <param-mmanon-ipv6-bits>`
     - .. include:: ../../reference/parameters/mmanon-ipv6-bits.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`embeddedIpv4.enable <param-mmanon-embeddedipv4-enable>`
     - .. include:: ../../reference/parameters/mmanon-embeddedipv4-enable.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`embeddedIpv4.anonMode <param-mmanon-embeddedipv4-anonmode>`
     - .. include:: ../../reference/parameters/mmanon-embeddedipv4-anonmode.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`embeddedIpv4.bits <param-mmanon-embeddedipv4-bits>`
     - .. include:: ../../reference/parameters/mmanon-embeddedipv4-bits.rst
        :start-after: .. summary-start
        :end-before: .. summary-end

.. toctree::
   :hidden:

   ../../reference/parameters/mmanon-ipv4-enable
   ../../reference/parameters/mmanon-ipv4-mode
   ../../reference/parameters/mmanon-ipv4-bits
   ../../reference/parameters/mmanon-ipv4-replacechar
   ../../reference/parameters/mmanon-ipv6-enable
   ../../reference/parameters/mmanon-ipv6-anonmode
   ../../reference/parameters/mmanon-ipv6-bits
   ../../reference/parameters/mmanon-embeddedipv4-enable
   ../../reference/parameters/mmanon-embeddedipv4-anonmode
   ../../reference/parameters/mmanon-embeddedipv4-bits


See Also
========

-  `Howto anonymize messages that go to specific
   files <http://www.rsyslog.com/howto-anonymize-messages-that-go-to-specific-files/>`_


Caveats/Known Bugs
==================

-  will **not** anonymize addresses in the header


Examples
========

Anonymizing messages
--------------------

In this snippet, we write one file without anonymization and another one
with the message anonymized. Note that once mmanon has run, access to
the original message is no longer possible (except if stored in user
variables before anonymization).

.. code-block:: none

   module(load="mmanon")
   action(type="omfile" file="/path/to/non-anon.log")
   action(type="mmanon" ipv6.enable="off")
   action(type="omfile" file="/path/to/anon.log")


Anonymizing a specific part of the ip address
---------------------------------------------

This next snippet is almost identical to the first one, but here we
anonymize the full IPv4 address. Note that by modifying the number of
bits, you can anonymize different parts of the address. Keep in mind
that in simple mode (used here), the bit values must match IP address
bytes, so for IPv4 only the values 8, 16, 24 and 32 are valid. Also, in
this example the replacement is done via asterisks instead of lower-case
"x"-letters. Also keep in mind that "replacementChar" can only be set in
simple mode.

.. code-block:: none

   module(load="mmanon") action(type="omfile" file="/path/to/non-anon.log")
   action(type="mmanon" ipv4.bits="32" ipv4.mode="simple" replacementChar="\*" ipv6.enable="off")
   action(type="omfile" file="/path/to/anon.log")


Anonymizing an odd number of bits
---------------------------------

The next snippet is also based on the first one, but anonymizes an "odd"
number of bits, 12. The value of 12 is used by some folks as a
compromise between keeping privacy and still permitting to gain some more
in-depth insight from log files. Note that anonymizing 12 bits may be
insufficient to fulfill legal requirements (if such exist).

.. code-block:: none

   module(load="mmanon") action(type="omfile" file="/path/to/non-anon.log")
   action(type="mmanon" ipv4.bits="12" ipv6.enable="off") action(type="omfile"
   file="/path/to/anon.log")


Anonymizing ipv4 and ipv6 addresses
-----------------------------------

You can also anonymize IPv4 and IPv6 in one go using a configuration like this.

.. code-block:: none

   module(load="mmanon") action(type="omfile" file="/path/to/non-anon.log")
   action(type="mmanon" ipv4.bits="12" ipv6.bits="128" ipv6.anonmode="random") action(type="omfile"
   file="/path/to/anon.log")


Anonymizing with default values
-------------------------------

It is also possible to use the default configuration for both types of
anonymization. This will result in IPv4 addresses being anonymized in zero
mode anonymizing 16 bits. IPv6 addresses will also be anonymized in zero
mode anonymizing 96 bits.

.. code-block:: none

   module(load="mmanon")
   action(type="omfile" file="/path/to/non-anon.log")
   action(type="mmanon")
   action(type="omfile" file="/path/to/anon.log")


Anonymizing only ipv6 addresses
-------------------------------

Another option is to only anonymize IPv6 addresses. When doing this you have to
disable IPv4 anonymization. This example will lead to only IPv6 addresses anonymized
(using the random-consistent mode).

.. code-block:: none

   module(load="mmanon")
   action(type="omfile" file="/path/to/non-anon.log")
   action(type="mmanon" ipv4.enable="off" ipv6.anonmode="random-consistent")
   action(type="omfile" file="/path/to/anon.log")

