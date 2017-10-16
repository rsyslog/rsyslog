IP Address Anonymization Module (mmanon)
========================================

**Module Name**: mmanon

**Author:** Rainer Gerhards <rgerhards@adiscon.com>

**Available since**: 7.3.7

**Description**:

The mmanon module permits to anonymize IP addresses. It is a message
modification module that actually changes the IP address inside the
message, so after calling mmanon, the original message can no longer be
obtained. Note that anonymization will break digital signatures on the
message, if they exist.

Please note that log files can also be anonymized via
`SLFA <https://github.com/jgerhards/SLFA/>`_ after they
have been created.

*How are IP-Addresses defined?*

We assume that an IPv4 address consists of four octets in dotted notation,
where each of the octets has a value between 0 and 255, inclusively.

An IPv6 is defined by being bewtween zero and eight hex values between 0
and ffff. These are separated by ':'. Leading zeros in blocks can be omitted
and blocks full of zeros can be abbreviated by using '::'. However, this
can ony happen once in an IP address.

 

**Module Configuration Parameters**:

Currently none.

 

**Action Confguration Parameters**:

Parameters starting with 'IPv4.' will configure IPv4 anonymization,
while 'IPv6.' parameters do the same for IPv6 anonymization.

-  **ipv4.enable** - default "on"

   Allows to enable or disable the anonymization of IPv4 addresses.

-  **ipv4.mode** - default "zero"

   There exist the "simple", "random", "random-consitent", and "zero"
   modes. In simple mode, only octets as whole can be anonymized
   and the length of the message is never changed. This means
   that when the last three octets of the address 10.1.12.123 are
   anonymized, the result will be 10.0.00.000. This means that
   the length of the original octets is still visible and may be used
   to draw some privacy-evasive conclusions. This mode is slightly
   faster than the other modes, and this may matter in high
   throughput environments.

   The modes "random" and "random-consistent" are very similar, in
   that they both anonymize ip-addresses by randomizing the last bits (any
   number) of a given address. However, while "random" mode assigns a new
   random ip-address for every address in a message, "random-consitent" will
   assign the same randomized address to every instance of the same original address.

   The default "zero" mode will do full anonymization of any number
   of bits and it will also normalize the address, so that no information
   about the original IP address is available. So in the above example,
   10.1.12.123 would be anonymized to 10.0.0.0.

-  **ipv4.bits** - default "16"

   This sets the number of bits that should be anonymized (bits are from
   the right, so lower bits are anonymized first). This setting permits
   to save network information while still anonymizing user-specific
   data. The more bits you discard, the better the anonymization
   obviously is. The default of 16 bits reflects what German data
   privacy rules consider as being sufficinetly anonymized. We assume,
   this can also be used as a rough but conservative guideline for other
   countries.
   Note: when in simple mode, only bits on a byte boundary can be
   specified. As such, any value other than 8, 16, 24 or 32 is invalid.
   If an invalid value is given, it is rounded to the next byte boundary
   (so we favor stronger anonymization in that case). For example, a bit
   value of 12 will become 16 in simple mode (an error message is also
   emitted).

-  **ipv4.replaceChar** - default "x"

   In simple mode, this sets the character that the to-be-anonymized
   part of the IP address is to be overwritten with. In any other
   mode the parameter is ignored if set.

-  **ipv6.enable** - default "on"

   Allows to enable or disable the anonymization of IPv6 addresses.

-  **ipv6.anonmode** - default "zero"

   This defines the mode, in which IPv6 addresses will be anonymized.
   There exist the "random", "random-consitent", and "zero" modes.

   The modes "random" and "random-consistent" are very similar, in
   that they both anonymize ip-addresses by randomizing the last bits (any
   number) of a given address. However, while "random" mode assigns a new
   random ip-address for every address in a message, "random-consitent" will
   assign the same randomized address to every instance of the same original address.

   The default "zero" mode will do full anonymization of any number
   of bits and it will also normalize the address, so that no information
   about the original IP address is available.

   Also note that an anonymmized IPv6 address will be normalized, meaning
   there will be no abbreviations, leading zeros will **not** be displayed,
   and capital letters in the hex numerals will be lowercase.

-  **ipv6.bits** - default "96"

   This sets the number of bits that should be anonymized (bits are from
   the right, so lower bits are anonymized first). This setting permits
   to save network information while still anonymizing user-specific
   data. The more bits you discard, the better the anonymization
   obviously is. The default of 96 bits reflects what German data
   privacy rules consider as being sufficinetly anonymized. We assume,
   this can also be used as a rough but conservative guideline for other
   countries.

**See Also**

-  `Howto anonymize messages that go to specific
   files <http://www.rsyslog.com/howto-anonymize-messages-that-go-to-specific-files/>`_

**Caveats/Known Bugs:**

-  will **not** anonymize addresses in the header

**Samples:**

In this snippet, we write one file without anonymization and another one
with the message anonymized. Note that once mmanon has run, access to
the original message is no longer possible (execept if stored in user
variables before anonymization).

::

  module(load="mmanon")
  action(type="omfile" file="/path/to/non-anon.log")
  action(type="mmanon" ipv6.enable="off")
  action(type="omfile" file="/path/to/anon.log")

This next snippet is almost identical to the first one, but here we
anonymize the full IPv4 address. Note that by modifying the number of
bits, you can anonymize different parts of the address. Keep in mind
that in simple mode (used here), the bit values must match IP address
bytes, so for IPv4 only the values 8, 16, 24 and 32 are valid. Also, in
this example the replacement is done via asterisks instead of lower-case
"x"-letters. Also keep in mind that "replacementChar" can only be set in
simple mode.

::

  module(load="mmanon") action(type="omfile" file="/path/to/non-anon.log")
  action(type="mmanon" ipv4.bits="32" ipv4.mode="simple" replacementChar="\*" ipv6.enable="off")
  action(type="omfile" file="/path/to/anon.log")

The next snippet is also based on the first one, but anonymizes an "odd"
number of bits, 12. The value of 12 is used by some folks as a
compromise between keeping privacy and still permiting to gain some more
in-depth insight from log files. Note that anonymizing 12 bits may be
insufficient to fulfill legal requirements (if such exist).

::

  module(load="mmanon") action(type="omfile" file="/path/to/non-anon.log")
  action(type="mmanon" ipv4.bits="12" ipv6.enable="off") action(type="omfile"
  file="/path/to/anon.log")

You can also anonymize IPv4 and IPv6 in one go using a configuration like this.

::

  module(load="mmanon") action(type="omfile" file="/path/to/non-anon.log")
  action(type="mmanon" ipv4.bits="12" ipv6.bits="128" ipv6.anonmode="random") action(type="omfile"
  file="/path/to/anon.log")

It is also possible to use the default configuration for both types of anonymization.
This will result in IPv4 addresses being anonymized in zero mode anonymizing 16 bits.
IPv6 addresses will also be anonymized in zero mode anonymizing 96 bits.

::

  module(load="mmanon")
  action(type="omfile" file="/path/to/non-anon.log")
  action(type="mmanon")
  action(type="omfile" file="/path/to/anon.log")

Another option is to only anonymize IPv6 addresses. When doing this you have to
disable IPv4 aonymization. This example will lead to only IPv6 addresses anonymized
(using the random-consistent mode).

::

  module(load="mmanon")
  action(type="omfile" file="/path/to/non-anon.log")
  action(type="mmanon" ipv4.enable="off" ipv6.anonmode="random-consistent")
  action(type="omfile" file="/path/to/anon.log")

This documentation is part of the `rsyslog <http://www.rsyslog.com/>`_
project.
Copyright © 2008-2013 by `Rainer
Gerhards <http://www.gerhards.net/rainer>`_ and
`Adiscon <http://www.adiscon.com/>`_. Released under the GNU GPL version
3 or higher.
