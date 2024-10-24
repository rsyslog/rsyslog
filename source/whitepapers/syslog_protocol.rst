syslog-protocol support in rsyslog
==================================

`rsyslog <http://www.rsyslog.com/>`_ **provides a trial implementation
of the proposed**
`syslog-protocol <http://www.monitorware.com/Common/en/glossary/syslog-protocol.php>`_
**standard.** The intention of this implementation is to find out what
inside syslog-protocol is causing problems during implementation. As
syslog-protocol is a standard under development, its support in rsyslog
is highly volatile. It may change from release to release. So while it
provides some advantages in the real world, users are cautioned against
using it right now. If you do, be prepared that you will probably need
to update all of your rsyslogds with each new release. If you try it
anyhow, please provide feedback as that would be most beneficial for us.

Currently supported message format
----------------------------------

Due to recent discussion on syslog-protocol, we do not follow any
specific revision of the draft but rather the candidate ideas. The
format supported currently is:

**``<PRI>VERSION SP TIMESTAMP SP HOSTNAME SP APP-NAME SP PROCID SP MSGID SP [SD-ID]s  SP MSG``**

Field syntax and semantics are as defined in IETF I-D
syslog-protocol-15.

Capabilities Implemented
------------------------

-  receiving message in the supported format (see above)
-  sending messages in the supported format
-  relaying messages
-  receiving messages in either legacy or -protocol format and
   transforming them into the other one
-  virtual availability of TAG, PROCID, APP-NAME, MSGID, SD-ID no matter
   if the message was received via legacy format, API or syslog-protocol
   format (non-present fields are being emulated with great success)
-  maximum message size is set via preprocessor #define
-  syslog-protocol messages can be transmitted both over UDP and plain
   TCP with some restrictions on compliance in the case of TCP

Findings
--------

This lists what has been found during implementation:

-  The same receiver must be able to support both legacy and
   syslog-protocol syslog messages. Anything else would be a big
   inconvenience to users and would make deployment much harder. The
   detection must be done automatically (see below on how easy that is).
-  **NUL characters inside MSG** cause the message to be truncated at
   that point. This is probably a major point for many C-based
   implementations. No measures have yet been taken against this.
   Modifying the code to "cleanly" support NUL characters is
   non-trivial, even though rsyslogd already has some byte-counted
   string library (but this is new and not yet available everywhere).
-  **character encoding in MSG**: it is problematic to do the right
   UTF-8 encoding. The reason is that we pick up the MSG from the local
   domain socket (which got it from the syslog(3) API). The text
   obtained does not include any encoding information, but it does
   include non US-ASCII characters. It may also include any other
   encoding. Other than by guessing based on the provided text, I have
   no way to find out what it is. In order to make the syslogd do
   anything useful, I have now simply taken the message as is and
   stuffed it into the MSG part. Please note that I think this will be a
   route that other implementors would take, too.
-  A minimal parser is easy to implement. It took me roughly 2 hours to
   add it to rsyslogd. This includes the time for restructuring the code
   to be able to parse both legacy syslog as well as syslog-protocol.
   The parser has some restrictions, though

   -  STRUCTURED-DATA field is extracted, but not validated. Structured
      data "[test ]]" is not caught as an error. Nor are any other
      errors caught. For my needs with this syslogd, that level of
      structured data processing is probably sufficient. I do not want
      to parse/validate it in all cases. This is also a performance
      issue. I think other implementors could have the same view. As
      such, we should not make validation a requirement.
   -  MSG is not further processed (e.g. Unicode not being validated)
   -  the other header fields are also extracted, but no validation is
      performed right now. At least some validation should be easy to
      add (not done this because it is a proof-of-concept and scheduled
      to change).

-  Universal access to all syslog fields (missing ones being emulated)
   was also quite easy. It took me around another 2 hours to integrate
   emulation of non-present fields into the code base.
-  The version at the start of the message makes it easy to detect if we
   have legacy syslog or syslog-protocol. Do NOT move it to somewhere
   inside the middle of the message, that would complicate things. It
   might not be totally fail-safe to just rely on "1 " as the "cookie"
   for a syslog-protocol. Eventually, it would be good to add some more
   uniqueness, e.g. "@#1 ".
-  I have no (easy) way to detect truncation if that happens on the UDP
   stack. All I see is that I receive e.g. a 4K message. If the message
   was e.g. 6K, I received two chunks. The first chunk (4K) is correctly
   detected as a syslog-protocol message, the second (2K) as legacy
   syslog. I do not see what we could do against this. This questions
   the usefulness of the TRUNCATE bit. Eventually, I could look at the
   UDP headers and see that it is a fragment. I have looked at a network
   sniffer log of the conversation. This looks like two
   totally-independent messages were sent by the sender stack.
-  The maximum message size is currently being configured via a
   preprocessor #define. It can easily be set to 2K or 4K, but more than
   4K is not possible because of UDP stack limitations. Eventually, this
   can be worked around, but I have not done this yet.
-  rsyslogd can accept syslog-protocol formatted messages but is able to
   relay them in legacy format. I find this a must in real-life
   deployments. For this, I needed to do some field mapping so that
   APP-NAME/PROCID are mapped into a TAG.
-  rsyslogd can also accept legacy syslog message and relay them in
   syslog-protocol format. For this, I needed to apply some sub-parsing
   of the TAG, which on most occasions provides correct results. There
   might be some misinterpretations but I consider these to be mostly
   non-intrusive.
-  Messages received from the syslog API (the normal case under \*nix)
   also do not have APP-NAME and PROCID and I must parse them out of TAG
   as described directly above. As such, this algorithm is absolutely
   vital to make things work on \*nix.
-  I have an issue with messages received via the syslog(3) API (or, to
   be more precise, via the local domain socket this API writes to):
   These messages contain a timestamp, but that timestamp does neither
   have the year nor the high-resolution time. The year is no real
   issue, I just take the year of the reception of that message. There
   is a very small window of exposure for messages read from the log
   immediately after midnight Jan 1st. The message in the domain socket
   might have been written immediately before midnight in the old year.
   I think this is acceptable. However, I can not assign a
   high-precision timestamp, at least it is somewhat off if I take the
   timestamp from message reception on the local socket. An alternative
   might be to ignore the timestamp present and instead use that one
   when the message is pulled from the local socket (I am talking about
   IPC, not the network - just a reminder...). This is doable, but
   eventually not advisable. It looks like this needs to be resolved via
   a configuration option.
-  rsyslogd already advertised its origin information on application
   startup (in a syslog-protocol-14 compatible format). It is fairly
   easy to include that with any message if desired (not currently
   done).
-  A big problem I noticed are malformed messages. In -syslog-protocol,
   we recommend/require to discard malformed messages. However, in
   practice users would like to see everything that the syslogd
   receives, even if it is in error. For the first version, I have not
   included any error handling at all. However, I think I would
   deliberately ignore any "discard" requirement. My current point of
   view is that in my code I would eventually flag a message as being
   invalid and allow the user to filter on this invalidness. So these
   invalid messages could be redirected into special bins.
-  The error logging recommendations (those I insisted on;)) are not
   really practical. My application has its own error logging philosophy
   and I will not change this to follow a draft.
-  Relevance of support for leap seconds and senders without knowledge
   of time is questionable. I have not made any specific provisions in
   the code nor would I know how to handle that differently. I could,
   however, pull the local reception timestamp in this case, so it might
   be useful to have this feature. I do not think any more about this
   for the initial proof-of-concept. Note it as a potential problem
   area, especially when logging to databases.
-  The HOSTNAME field for internally generated messages currently
   contains the hostname part only, not the FQDN. This can be changed
   inside the code base, but it requires some thinking so that thinks
   are kept compatible with legacy syslog. I have not done this for the
   proof-of-concept, but I think it is not really bad. Maybe an hour or
   half a day of thinking.
-  It is possible that I did not receive a TAG with legacy syslog or via
   the syslog API. In this case, I can not generate the APP-NAME. For
   consistency, I have used "-" in such cases (just like in PROCID,
   MSGID and STRUCTURED-DATA).
-  As an architectural side-effect, syslog-protocol formatted messages
   can also be transmitted over non-standard syslog/raw tcp. This
   implementation uses the industry-standard LF termination of tcp
   syslog records. As such, syslog-protocol messages containing a LF
   will be broken invalidly. There is nothing that can be done against
   this without specifying a TCP transport. This issue might be more
   important than one thinks on first thought. The reason is the wide
   deployment of syslog/tcp via industry standard.

**Some notes on syslog-transport-udp-06**

-  I did not make any low-level modifications to the UDP code and think
   I am still basically covered with this I-D.
-  I deliberately violate section 3.3 insofar as that I do not
   necessarily accept messages destined to port 514. This feature is
   user-required and a must. The same applies to the destination port. I
   am not sure if the "MUST" in section 3.3 was meant that this MUST be
   an option, but not necessarily be active. The wording should be
   clarified.
-  section 3.6: I do not check checksums. See the issue with discarding
   messages above. The same solution will probably be applied in my
   code.

 

Conclusions/Suggestions
-----------------------

These are my personal conclusions and suggestions. Obviously, they must
be discussed ;)

-  NUL should be disallowed in MSG
-  As it is not possible to definitely know the character encoding of
   the application-provided message, MSG should **not** be specified to
   use UTF-8 exclusively. Instead, it is suggested that any encoding may
   be used but UTF-8 is preferred. To detect UTF-8, the MSG should start
   with the UTF-8 byte order mask of "EF BB BF" if it is UTF-8 encoded
   (see section 155.9 of
   `https://www.unicode.org/versions/Unicode4.0.0/ch15.pdf <https://www.unicode.org/versions/Unicode4.0.0/ch15.pdf>`_)
-  Requirements to drop messages should be reconsidered. I guess I would
   not be the only implementor ignoring them.
-  Logging requirements should be reconsidered and probably be removed.
-  It would be advisable to specify "-" for APP-NAME is the name is not
   known to the sender.
-  The implications of the current syslog/tcp industry standard on
   syslog-protocol should be further evaluated and be fully understood

 

