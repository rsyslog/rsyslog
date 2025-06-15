FAQ: some general topics often asked
====================================


.. _faq_message_duplication:

FAQ: Message Duplication with rsyslog
-------------------------------------

**Q: Why do I see message duplication with rsyslog?**

**A:** rsyslog follows an "at least once" delivery principle, meaning it's possible to encounter some message duplication. This typically occurs when forwarding data and the connection is interrupted. This is often the case when load balancers are involved.

One common scenario involves the `omfwd` module with TCP. If the connection breaks, `omfwd` cannot precisely determine which messages were successfully stored by the remote peer, leading to potential resending of more messages than necessary. To mitigate this, consider using the `omrelp` module, which provides reliable event logging protocol (RELP) and ensures exact message delivery without duplication.

While the `omfwd` case is common, other configurations might also cause duplication. Always ensure that your queue and retry settings are properly configured to minimize this issue.
