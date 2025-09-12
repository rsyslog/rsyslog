# Documentation for the rsyslog project

Documentation for rsyslog is generated with the (Python) Sphinx documentation
processor. Documentation for the rsyslog project itself is provided
by this README and other documentation linked from this file.

## Learning the doc tools

If you're new to rst and Sphinx, see the Sphinx documentation to get started:
http://www.sphinx-doc.org/en/stable/contents.html

## Contributed Software/Content

In the repo, you'll find a `doc/contrib` directory. This directory contains
content contributed by community members. All content in the `rsyslog`
repository, including contributions, is subject to review processes for quality
and adherence to project standards.

---

## Dev Team resources

In addition to the directions here, there's also a separate
[BUILDS_README.md](doc/BUILDS_README.md) file for use by `rsyslog` team
members. This doc serves as a quick reference for those who regularly
provide dev and official release builds of the documentation.

---

## Contributing to the docs

1.  Log in with a GitHub account.
2.  Fork the official https://github.com/rsyslog/rsyslog repo.
3.  Create a new branch off of the latest `main` branch.
4.  Make your changes in the **`doc/`** subfolder.
5.  Commit to the new branch in your fork.
6.  Submit a Pull Request (PR) for review
    (https://github.com/rsyslog/rsyslog/pulls). **Note that PRs will be automatically AI reviewed.**
7.  Stop making any changes to your new branch now that you've submitted a
    Pull Request for review. Instead, create a new branch from your `main`
    branch while you wait for feedback from the doc team.
8.  A team member will review and offer feedback on your work. After
    feedback has been given and you've made all necessary changes, your
    PR will be accepted and merged into the official `main` branch.
9.  At this point, delete the branch you submitted the PR from and start
    a new one for your next round of work.

For small changes, you can do all the work through the GitHub web
interface. For larger changes, some familiarity with Git is useful, though
editors like Atom or Visual Studio Code make interfacing with Git
easier for newcomers.

Before you begin your work, we encourage you to review existing PRs and
open issues so you can coordinate your work with other contributors.

Please reach out if you have any questions as you work through making your
changes.

**Tip:** If you'd like something less complex to get started with, please see
issues tagged with
[good first issue](https://github.com/rsyslog/rsyslog/labels/good%20first%20issue)
or [help wanted](https://github.com/rsyslog/rsyslog/labels/help%20wanted).

---

## Requesting feedback/help

While working on changes to the docs, we encourage you to seek input from
other members of the community. You can do this via the mailing list, here
on GitHub by submitting a new issue, or (experimentally) by [posting a question
to Stack Exchange](https://serverfault.com/questions/ask?tags=rsyslog).

-   Mailing list: http://lists.adiscon.net/mailman/listinfo/rsyslog
-   Stack Exchange (experimental)
    -   [Ask a question](https://serverfault.com/questions/ask?tags=rsyslog)
    -   [View existing questions](https://stackexchange.com/filters/327462/rsyslog)

---

## Building the documentation

These directions assume a modern Python 3 environment (Python 3.8+; 3.10+ recommended).

### Assumptions

-   You want to install the `pip` Python package as a standard user, which places
    installed packages into that user's home directory. Remove the `--user`
    flag if you want to install system-wide for all users instead.

-   You want to use a virtual environment to install Sphinx and its dependencies
    into a dedicated environment instead of installing alongside packages that
    were installed system-wide or to the user's home directory with the `--user`
    flag. If you want to install the `sphinx` package and all dependent packages
    for all users of the system, then you'll need to run the package
    installation commands as an elevated user account (e.g., `sudo`, `su`, or
    with administrator rights on a Windows system).

-   You're running through these steps for the first time. Skip the steps
    involving installation of packages and applications if you're just generating an updated
    copy of the documentation.

### Prep environment

The first part of the process differs slightly depending on your OS. The
later steps are identical, so we've covered those steps in one place.

#### Linux

1.  Download the pip installer from https://bootstrap.pypa.io/get-pip.py.
2.  Install `pip` locally instead of system-wide:
        1.  `python3 ./get-pip.py --user`
3.  Install the `virtualenv` package and create a new virtual environment:
        1.  `python3 -m pip install virtualenv --user`
        1.  `python3 -m virtualenv rsyslog-docs-build`
    1.  `source rsyslog-docs-build/bin/activate`
4.  Install `git` for your distro. Since distros name the package differently,
    you may need to substitute the package name from the examples
    below with the name of the package for your distro.

    You'll need to install Git to clone the project repo, manage
    your changes, and contribute them back for review and eventual inclusion
    in the project.

    Example commands for installing Git:

    -   Debian/Ubuntu: `apt-get install git-core`
    -   CentOS/RHEL: `yum install git`

    See the
    [Installing Git](https://git-scm.com/book/en/v2/Getting-Started-Installing-Git)
    chapter from [Pro Git 2](https://git-scm.com/book/) for additional examples.

#### Windows

1.  Download the pip installer from https://bootstrap.pypa.io/get-pip.py.
2.  Download and install Git for Windows from https://git-scm.com/download/win.
3.  Install `pip` locally instead of system-wide:
        1.  `python get-pip.py --user`
4.  Install the `virtualenv` package and create a new virtual environment:
        1.  `python -m pip install virtualenv --user`
        1.  `python -m virtualenv rsyslog-docs-build`
    1.  `rsyslog-docs-build\Scripts\activate.bat`

#### Windows and Linux

1.  Install the `sphinx` package and any other project dependencies in our
    new virtual environment instead of system-wide:
        1.  `python -m pip install -r requirements.txt`
2.  Clone the official Git repo:
    1.  `git clone https://github.com/rsyslog/rsyslog.git`
3.  Check out either the current stable or development (aka, "main") branch:
    1.  `cd rsyslog/doc`
    1.  `git checkout BRANCH_NAME_HERE`
        -   Choose the `v8-stable` branch for coverage of features currently
            available in the latest stable release.
        -   Choose the `main` branch for coverage of upcoming features and fixes.
4.  **Optional:** If you've previously cloned the repo, run `git pull` to update it
    with new changes before continuing.

### Generate documentation

1.  Generate HTML format:
    1.  `sphinx-build -b html source build`
2.  Generate EPUB format:
    1.  `sphinx-build -b epub source build`
3.  Review generated contents:
    -   Open `rsyslog/doc/build/index.html` in a browser.
    -   Use Calibre, Microsoft Edge, Okular, Google Play Books, or any other
        EPUB compatible reader to view the `rsyslog/doc/build/rsyslog.epub` file.

---

## CI deployment to GitHub Pages

A GitHub Actions workflow automatically builds and deploys documentation previews for pull requests and updates the main documentation site.

-   **Pull Request Previews**:
    -   Triggered by changes to doc/**/*.rst files in a PR.
    -   Builds docs with make html SPHINXOPTS="-W -q --keep-going".
    -   For same-repo PRs, a preview is deployed to GitHub Pages (e.g., .../pr-<PR_NUMBER>/) and a link is posted as a PR comment.
    -   For forked PRs, deployment is skipped due to permissions, but the built HTML is available as a downloadable artifact.
-   **Main Branch Deployment**:
    -   Triggered by pushes to main/master or manual runs.
    -   Builds docs with make html SPHINXOPTS="-W", treating warnings as errors.
    -   Deploys to the main GitHub Pages site and uploads the build as an artifact.
