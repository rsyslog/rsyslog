Version Naming
==============

This is the proposal on how versions should be named in the future:

Rsyslog version naming has undergone a number of changes in the past.
Our sincere hopes is that the scheme outlined here will serve us well
for the future. In general, a three-number versioning scheme with a
potential development state indication is used. It follows this pattern:

major.minor.patchlevel[-devstate]

where devstate has some further structure:
-<releaseReason><releaseNumber>

All stable builds come without the devstate part. All unstable
development version come with it.

The major version is incremented whenever something really important
happens. A single new feature, even if important, does not justify an
increase in the major version. There is no hard rule when the major
version needs an increment. It mostly is a soft factor, when the
developers and/or the community think there has been sufficient change
to justify that. Major version increments are expected to happen quite
infrequently, maybe around once a year. A major version increment has
important implications from the support side: without support contracts,
the current major version's last stable release and the last stable
release of the version immediately below it are supported (Adiscon, the
rsyslog sponsor, offers `support contracts <http://www.rsyslog.com/professional-services/>`_
covering all other versions).

The minor version is incremented whenever a non-trivial new feature is
planned to be added. Triviality of a feature is simply determined by
time estimated to implement a feature. If that's more than a few days,
it is considered a non-trivial feature. Whenever a new minor version is
begun, the desired feature is identified and will be the primary focus
of that major.minor version. Trivial features may justify a new minor
version if they either do not look trivial from the user's point of view
or change something quite considerable (so we need to alert users). A
minor version increment may also be done for some other good reasons
that the developers have.

The patchlevel is incremented whenever there is a bugfix or very minor
feature added to a (stable or development) release.

The devstate is important during development of a feature. It helps the
developers to release versions with new features to the general public
and in the hope that this will result in some testing. To understand how
it works, we need to look at the release cycle: As already said, at the
start of a new minor version, a new non-trivial feature to be
implemented in that version is selected. Development on this feature
begins. At the current pace of development, getting initial support for
such a non-trivial feature typically takes between two and four weeks.
During this time, new feature requests come in. Also, we may find out
that it may be just the right time to implement some not yet targeted
feature requests. A reason for this is that the minor release's feature
focus is easier to implement if the other feature is implemented first.
This is a quite common thing to happen. So development on the primary
focus may hold for a short period while we implement something else.
Even unrelated, but very trivial feature requests (maybe an hour's worth
of time to implement), may be done in between. Once we have implemented
these things, we would like to release as quickly as possible (even more
if someone has asked for the feature). So we do not like to wait for the
original focus feature to be ready (what could take maybe three more
weeks). As a result, we release the new features. But that version will
also include partial code of the focus feature. Typically this doesn't
hurt as long as noone tries to use it (what of course would miserably
fail). But still, part of the new code is already in it. When we release
such a "minor-feature enhanced" but "focus-feature not yet completed"
version, we need a way to flag it. In current thinking, that is using a
"-mf<version>" devstate in the version number ("mf" stands for "minor
feature"). Version numbers for -mf releases start at 0 for the first
release and are monotonically incremented. Once the focus feature has
been fully implemented, a new version now actually supporting that
feature will be released. Now, the release reason is changed to the
well-know "-rc<version>" where "rc" stands for release candidate. For
the first release candidate, the version starts at 0 again and is
incremented monotonically for each subsequent release. Please note that
a -rc0 may only have bare functionality but later -rc's have a richer
one. If new minor features are implemented and released once we have
reached rc stage, still a new rc version is issued. The difference
between "mf" and "rc" is simply the presence of the desired feature. No
support is provided for -mf versions once the first -rc version has been
released. And only the most current -rc version is supported.

The -rc is removed and the version declared stable when we think it has
undergone sufficient testing and look sufficiently well. Then, it'll
turn into a stable release. Stable minor releases never receive
non-trivial new features. There may be more than one -rc releases
without a stable release present at the same time. In fact, most often
we will work on the next minor development version while the previous
minor version is still a -rc because it is not yet considered
sufficiently stable.

Note: the absence of the -devstate part indicates that a release is
stable. Following the same logic, any release with a -devstate part is
unstable.

A quick sample: 

4.0.0 is the stable release. We begin to implement relp, moving to
major.minor to 4.1. While we develop it, someone requests a trivial
feature, which we implement. We need to release, so we will have
4.1.0-mf0. Another new feature is requested, move to 4.1.0-mf2. A first
version of RELP is implemented: 4.1.0-rc0. A new trivial feature is
implemented: 4.1.0-rc1. Relp is being enhanced: 4.1.0-rc2. We now feel
RELP is good enough for the time being and begin to implement TLS on
plain /Tcp syslog: logical increment to 4.2. Now another new feature in
that tree: 4.2.0-mf0. Note that we now have 4.0.0 (stable) and 4.1.0-rc2
and 4.1.0-mf0 (both devel). We find a big bug in RELP coding. Two new
releases: 4.1.0-rc3, 4.2.0-mf1 (the bug fix acts like a non-focus
feature change). We release TLS: 4.2.0-rc0. Another RELP bug fix
4.1.0-rc4, 4.2.0-rc1. After a while, RELP is matured: 4.1.0 (stable).
Now support for 4.0.x stable ends. It, however, is still provided for
3.x.x (in the actual case 2.x.x, because v3 was under the old naming
scheme and now stable v3 was ever released).

This is how it is done so far:

This document briefly outlines the strategy for naming versions. It
applies to versions 1.0.0 and above. Versions below that are all
unstable and have a different naming schema.

**Please note that version naming is currently being changed. There is a
`blog post about future rsyslog
versions <https://rainer.gerhards.net/2007/08/on-rsyslog-versions.html>`_.**

The major version is incremented whenever a considerate, major features
have been added. This is expected to happen quite infrequently.

The minor version number is incremented whenever there is "sufficient
need" (at the discretion of the developers). There is a notable
difference between stable and unstable branches. The **stable branch**
always has a minor version number in the range from 0 to 9. It is
expected that the stable branch will receive bug and security fixes
only. So the range of minor version numbers should be quite sufficient.

For the **unstable branch**, minor version numbers always start at 10
and are incremented as needed (again, at the discretion of the
developers). Here, new minor versions include both fixes as well as new
features (hopefully most of the time). They are expected to be released
quite often.

The patch level (third number) is incremented whenever a really minor
thing must be added to an existing version. This is expected to happen
quite infrequently.

In general, the unstable branch carries all new development. Once it
concludes with a sufficiently-enhanced, quite stable version, a new
major stable version is assigned.
