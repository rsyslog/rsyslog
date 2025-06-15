# How to Contribute

Contributing is easy: fork this repo, create a new branch, change the doc
and then create a pull request.

The system will automatically initiate some tests on your contribution ("CI").
Please ensure that these checks succeed.

Prior to merge, a review will be made by a maintainer. If questions appear,
please be ready to address them.

For a successful contribution, it is important to follow up on the pull
request.

Authors who provided substantial technical content to a documentation page
can be mentioned (usually be mentioning themselves) on that page. They
may include a link to their homepage as well as their email address. The
same applies to rsyslog module authors. By default, the author should be named,
except if he or she does not want this. Module authors belong into the header
of the module documentation.

For links, the same policies as laid down below applies.

# Reviewing your PRs

After submitting your pull request, please check for CI errors and handle
them. **PRs with CI failures cannot be merged.**

The CI system also generates an actual HTML preview from your contribution.
This helps you (and others!) to review the ultimate outcome of the
change.

Due to technical reasons, the preview link is a bit hard to find. Thus
we have created a short video that tells you where to find it:

    https://youtu.be/26CN8hQGGJU

Note: previews are deleted after a couple of days. The exact retention
period depends on server load, but you should be able to view it for at least
three days after its creation (we actually try to keep it seven days).

# Contributing Content that already appeared Elsewhere

We are glad you consider adding content that you already published to the
rsyslog documentation. This makes your content more accessible and helps
rsyslog users. You are permitted to link back to the original appearance
of the content.

There are some easy rules for doing so:

- you must be the copyright holder
- the content must be technical and a useful addition to the existing
  doc (aka purely promotional or duplicated content is not permitted)
- there must be no monetization inside the submitted doc content itself
  (some monetization inside the original content is permitted as
  elaborated down below)
- you may provide up to two external links:
  - one to the author (person or organization)
  - one to the original place of appearance (blog, company site, ...)
- additional links may be included if the rsyslog-doc team considers
  them useful (e.g. another open source library). Links to RFCs, at the
  official IETF archive are always permitted and unproblematic.
- links must not lead to malicious, offensive or otherwise inappropriate
  sites (the validation is done by the rsyslog-doc maintainers and no
  complaints are accepted)

Note that similar rules exist for content that is destined to appear on
rsyslog.com. However, we nowadays try to limit documentation type of content
to the actual documentation.

The same rules also apply to content published via e.g. YouTube.com.
Monetized content on the original site (and in videos) is permitted as
long as the main focus is on the technical content. Again, the decision
of what is appropriate and not is solely made by the rsyslog-doc team.

The core guiding principle is that we would like to have useful content
which is of help of the user base. We want to make this process as easy
as possible.
