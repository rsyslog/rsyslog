How to Contribute
=================
Rsyslog is a real open source project and open to contributions.
By contributing, you help improve the state of logging as well as improve
your own professional profile. Contributing is easy, and there are options
for everyone - you do not need to be a developer.

These are many ways to contribute to the project:
 * become a rsyslog ambassador and let other people know about rsyslog and how to utilize it for best results. Help rsyslog getting backlinks, be present on Internet news sites or at meetings you attend.
 * help others by offering support on
   * the rsyslog forums at http://kb.monitorware.com/rsyslog-f40.html
   * the rsyslog mailing list at http://lists.adiscon.net/mailman/listinfo/rsyslog
 * help with the documentation; you can either contribute
   * to the [rsyslog doc directory](https://github.com/rsyslog/rsyslog/tree/master/doc), which is shown on http://rsyslog.com/doc
   * to the rsyslog project web site -- just ask us for account creation
   * on the rsyslog wiki at http://wiki.rsyslog.com/
 * become a bug-hunter and help with testing rsyslog development releases
 * help driving the rsyslog infrastructure with its web sites, wikis and the like
 * help creating packages
 * or, obviously, help with rsyslog code development

This list is not conclusive. There for sure are many more ways to contribute and if you find one, just let us know. We are very open to new suggestions and like to try out new things.


Requirements for patches
------------------------
In order to ensure good code quality, after applying the path the code must

- compile cleanly without WARNING messages under both gcc and clang
- pass clang static analyzer without any report

Note that both warning messages and static analyzer warnings may be false
positives. We have decided to accept that fate and work around it (e.g. by
re-arranging the code, etc). Otherwise, we cannot use these useful features.

As a last resort, compiler warnings can be turned off via
   #pragma diagnostic
directives. This should really only be done if there is no other known
way around it. If so, it should be applied to a single function, only and
not to full source file. Be sure to re-enable the warning after the function
in question. We have done this in some few cases ourselfs, and if someone
can fix the root cause, we would appreciate help. But, again, this is a
last resort which should normally not be used.

For pull requests submitted via github, these two conditions are 
verified automatically. See the PR for potential failueres. For patches
submitted otherwise, they will be verified semi-manually.

Also, patches are requested to not break the testbench. Unfortunately, the
current testbench has some racy tests, which are still useful enough so that
we do not want to disable them until the root cause has been found. If your
PR runs into something that you think is not related to your code, just sit
back and relax. The rsyslog core developer team reviews PRs regularly and
restarts tests which we know to look racy. If the problem persists, we will
contact you.

Once all this has passed, a final integration test will be done via buildbot
on a larger number of platforms. This is a semi-manual process. You will be
contacted if a problem shows up during it (very unlikely, but occasionally
happens).

Note to developers
------------------
Please adress pull requests against the master branch. The changes will at
first be merged into another branch (master-candidate). There, the
changes will be tested and merged into the master branch, should the test succeed.

More information is available at:
    http://www.rsyslog.com/how-to-contribute-to-rsyslog/
