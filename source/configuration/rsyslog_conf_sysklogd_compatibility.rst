sysklogd compatibility
======================

This is a part of the rsyslog.conf documentation.

`Back to rsyslog.conf manual <rsyslog_conf.html>`_

Rsyslog supports standard sysklogd's configuration file format and
extends it. So in general, you can take a "normal" syslog.conf and use
it together with rsyslogd. It will understand everything. However, to
use most of rsyslogd's unique features, you need to add extended
configuration directives.

Rsyslogd supports the classical, selector-based rule lines. They are
still at the heart of it and all actions are initiated via rule lines.
However, there are ample new directives, either in rsyslog traditional
format (starting with a dollar sign) or in RainerScript format. These
work together with sysklogd statements. A few select statements are no
longer supported and may generate error messages. They are mentioned in
the compatibility notes.

[`manual index <manual.html>`_\ ]
[`rsyslog.conf <rsyslog_conf.html>`_\ ] [`rsyslog
site <http://www.rsyslog.com/>`_\ ]

This documentation is part of the `rsyslog <http://www.rsyslog.com/>`_
project.
 Copyright © 2008-2013 by `Rainer
Gerhards <http://www.gerhards.net/rainer>`_ and
`Adiscon <http://www.adiscon.com/>`_. Released under the GNU GPL version
3 or higher.
