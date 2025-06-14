RSyslog - History
=================

Rsyslog is a GPL-ed, enhanced syslogd. Among others, it offers support
for reliable syslog over TCP, writing to MariaDB/MySQL databases and fully
configurable output formats (including great timestamps).

Rsyslog was initiated by `Rainer Gerhards <https://rainer.gerhards.net/>`_,
`Großrinderfeld <https://www.rainer-gerhards.de/grossrinderfeld/>`_. 
If you are interested to learn why Rainer initiated the project, you may want
to read his blog posting on "`why the world needs another syslogd 
<https://rainer.gerhards.net/2007/08/why-doesworld-need-another-syslogd.html>`_\ ".

Rsyslog was forked in **2004** from the `sysklogd standard
package <http://www.infodrom.org/projects/sysklogd/>`_. The goal of the
rsyslog project is to provide a feature-richer and reliable syslog
daemon while retaining drop-in replacement capabilities to stock
syslogd. By "reliable", we mean support for reliable transmission modes
like TCP or `RFC
3195 <http://www.monitorware.com/Common/en/glossary/rfc3195.php>`_
(syslog-reliable). We do NOT imply that the sysklogd package is
unreliable.

The name "rsyslog" stems back to the planned support for
syslog-reliable. Ironically, the initial release of rsyslog did NEITHER
support syslog-reliable NOR tcp based syslog. Instead, it contained
enhanced configurability and other enhancements (like database support).
The reason for this is that full support for RFC 3195 would require even
more changes and especially fundamental architectural changes. Also,
questions asked on the loganalysis list and at other places indicated
that RFC3195 is NOT a prime priority for users, but rather better
control over the output format. So there we were, with a rsyslogd that
covers a lot of enhancements, but not a single one of these that made
its name ;) Since version 0.9.2, receiving syslog messages via plain tcp
is finally supported, a bit later sending via TCP, too. Starting with
1.11.0, RFC 3195 is finally supported at the receiving side (a.k.a.
"listener"). Support for sending via RFC 3195 is still due. Anyhow,
rsyslog has come much closer to what it name promises.

The database support was initially included so that our web-based syslog
interface could be used. This is another open source project which can
be found under `https://loganalyzer.adiscon.com/ <https://loganalyzer.adiscon.com/>`_.
We highly recommend having a look at it. It might not work for you if
you expect thousands of messages per second (because your database won't
be able to provide adequate performance), but in many cases it is a very
handy analysis and troubleshooting tool. In the mean time, of course,
lots of people have found many applications for writing to databases, so
the prime focus is no longer on phpLogcon.

Rsyslogd supports an enhanced syslog.conf file format, and also works
with the standard syslog.conf. In theory, it should be possible to
simply replace the syslogd binary with the one that comes with rsyslog.
Of course, in order to use any of the new features, you must re-write
your syslog.conf. To learn how to do this, please review our commented
`sample.conf <sample.conf.php>`_ file. It outlines the enhancements over
stock syslogd. Discussion has often arisen of whether having an "old
syslogd" logfile format is good or evil. So far, this has not been
solved (but Rainer likes the idea of a new format), so we need to live
with it for the time being. It is planned to be reconsidered in the 3.x
release time frame.

If you are interested in the `IHE <https://en.wikipedia.org/wiki/Integrating_the_Healthcare_Enterprise>`_
environment, you might be interested to hear that rsyslog supports
message with sizes of 32k and more. This feature has been tested, but by
default is turned off (as it has some memory footprint that we didn't
want to put on users not actually requiring it). Search the file
syslogd.c and search for "IHE" - you will find easy and precise
instructions on what you need to change (it's just one line of code!).
Please note that RFC 3195/COOKED supports 1K message sizes only. It'll
probably support longer messages in the future, but it is our belief
that using larger messages with current RFC 3195 is a violation of the
standard.

In **February 2007**, 1.13.1 was released and served for quite a while
as a stable reference. Unfortunately, it was not later released as
stable, so the stable build became quite outdated.

In **June 2007**, Peter Vrabec from Red Hat helped us to create RPM
files for Fedora as well as supporting IPv6. There also seemed to be
some interest from the Red Hat community. This interest and new ideas
resulted in a very busy time with many great additions.

In **July 2007**, Andrew Pantyukhin added BSD ports files for rsyslog
and liblogging. We were strongly encouraged by this too. It looks like
rsyslog is getting more and more momentum. Let's see what comes next...

Also in **July 2007** (and beginning of August), Rainer remodeled the
output part of rsyslog. It got a clean object model and is now prepared
for a plug-in architecture. During that time, some base ideas for the
overall new object model appeared.

In **August 2007** community involvement grew more and more. Also, more
packages appeared. We were quite happy about that. To facilitate user
contributions, we set up a `wiki <http://wiki.rsyslog.com/>`_ on August
10th, 2007. Also in August 2007, rsyslog 1.18.2 appeared, which is
deemed to be quite close to the final 2.0.0 release. With its
appearance, the pace of changes was deliberately reduced, in order to
allow it to mature (see Rainers's `blog
post <https://rainer.gerhards.net/2007/07/pace-of-changes-in-rsyslog.html>`_
on this topic, written a bit early, but covering the essence).

In **November 2007**, rsyslog became the default syslogd in Fedora 8.
Obviously, that was something we \*really\* liked. Community involvement
also is still growing. There is one sad thing to note: ever since
summer, there is an extremely hard to find segfault bug. It happens on
very rare occasions only and never in lab. We are hunting this bug for
month now, but still could not get hold of it. Unfortunately, this also
affects the new features schedule. It makes limited sense to implement
new features if problems with existing ones are not really understood.

**December 2007** showed the appearance of a postgres output module,
contributed by sur5r. With 1.20.0, December is also the first time since
the bug hunt that we introduce other new features. It has been decided
that we carefully will add features in order to not affect the overall
project by these rare bugs. Still, the bug hunt is top priority, but we
need to have more data to analyze. At then end of December, it looked
like the bug was found (a race condition), but further confirmation from
the field is required before declaring victory. December also brings the
initial development on **rsyslog v3**, resulting in loadable input
modules, now running on a separate thread each.

On **January, 2nd 2008**, rsyslog 1.21.2 is re-released as rsyslog
v2.0.0 stable. This is a major milestone as far as the stable build is
concerned. v3 is not yet officially announced. Other than the stable v2
build, v3 will not be backwards compatible (including missing
compatibility to stock sysklogd) for quite a while. Config file changes
are required and some command line options do no longer work due to the
new design.

On January, 31st 2008 the new massively-multithreaded queue engine was
released for the first time. It was a major milestone, implementing a
feature I dreamed of for more than a year.

End of February 2008 saw the first note about RainerScript, a way to
configure rsyslogd via a script-language like configuration format. This
effort evolved out of the need to have complex expression support, which
was also the first use case. On February, 28th rsyslog 3.12.0 was
released, the first version to contain expression support. This also
meant that rsyslog from that date on supported all syslog-ng major
features, but had a number of major features exclusive to it. With
3.12.0, I consider rsyslog fully superior to syslog-ng (except for
platform support).

Be sure to visit Rainer's `syslog
blog <https://rainer.gerhards.net/>`_ to get some more insight into
the development and futures of rsyslog and syslog in general.
