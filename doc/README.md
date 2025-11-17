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

### Linux distribution packages

Install the base packages for your distro. Only major distros are listed here:

-   Debian/Ubuntu:
    -   `sudo apt update && sudo apt install -y python3 python3-venv python3-pip git`
-   Red Hat (RHEL/CentOS/Fedora):
    -   `sudo dnf install -y python3 python3-pip git`  (use `yum` on older releases)

These provide Python 3, `venv`/`pip`, and `git`. If `python3 -m venv` is unavailable on your platform, install `virtualenv` via `pip` and the scripts below will fall back to it automatically.

### Quickstart using make (recommended)

From the repository root:

- Build HTML (no sitemap):
  - `make -C doc html`
- Build HTML with sitemap (opt-in):
  - `make -C doc html-with-sitemap`
- Build single-page HTML (minimal layout):
  - `make -C doc singlehtml`
- List available builders/targets:
  - `make -C doc help`

Tips:
- Pass extra Sphinx options via `SPHINXOPTS`, e.g. warnings-as-errors:
  - `make -C doc html SPHINXOPTS="-W -q --keep-going"`
- Use parallel jobs: `make -C doc -j4 html` (the Makefile forwards `-j` to Sphinx).

### Quickstart using helper scripts

We provide simple scripts that create a virtual environment (if missing), install `doc/requirements.txt`, and build the docs. Prefer the Makefile targets above; use these scripts if `make` is unavailable.

-   Linux:
    -   `./doc/tools/build-doc-linux.sh --clean --format html`
    -   Options: `--strict` (treat warnings as errors), `--format html|epub`, `--extra "<opts>"` for extra `sphinx-build` options.
    -   Optional sitemap: add `--extra "-t with_sitemap"` to enable sitemap generation.
-   Windows (PowerShell):
    -   `powershell -ExecutionPolicy Bypass -File .\doc\tools\build-doc-windows.ps1 -Clean -Format html`
    -   Options: `-Strict`, `-Format html|epub`, `-Extra "<opts>"`.

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

If you prefer the manual route instead of the helper script above:

1.  Ensure distro packages are installed (see "Linux distribution packages" above).
2.  Create and activate a virtual environment:
        1.  `python3 -m venv rsyslog-docs-build || (python3 -m pip install --user virtualenv && python3 -m virtualenv rsyslog-docs-build)`
        1.  `source rsyslog-docs-build/bin/activate`
3.  Install Git if not present and proceed below.

#### Windows

If you prefer the manual route instead of the helper script above:

1.  Install Python 3 from `https://www.python.org/downloads/` and Git for Windows from `https://git-scm.com/download/win`.
2.  Create and activate a virtual environment:
        1.  `python -m venv rsyslog-docs-build` (or `python -m pip install --user virtualenv && python -m virtualenv rsyslog-docs-build`)
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

1.  Generate HTML format (Makefile preferred):
    -   `make -C doc html` (no sitemap)
    -   `make -C doc html-with-sitemap` (includes sitemap)
    -   Fallback (no Makefile available): `sphinx-build -b html source build`
        -   With sitemap: add `-t with_sitemap`
2.  Generate EPUB format:
    -   `make -C doc epub`
    -   Fallback: `sphinx-build -b epub source build`
3.  Review generated contents:
    -   Makefile builds: open `rsyslog/doc/build/html/index.html`.
    -   Direct Sphinx builds: open `rsyslog/doc/build/index.html`.
    -   Use any EPUB reader to view `rsyslog/doc/build/rsyslog.epub`.

---

### JSON-LD metadata and author resolution

JSON-LD is injected by default for HTML documentation builds. To skip emitting it
(useful for package maintainers who want to minimize offline footprint), run the
build with `DISABLE_JSON_LD=1` in the environment before invoking Sphinx or
`make -C doc html`.【F:doc/source/conf.py†L371-L627】 Author detection for that JSON-LD
block follows this order:

1. Use `:author:` or `:authors:` from a page's ``.. meta::`` block. If multiple
   authors are provided, only the first is used.【F:doc/source/conf.py†L593-L603】
2. Fall back to the page context's ``author`` value if Sphinx sets one.
3. Fall back to the global `author` configured in ``conf.py``
   (``Rainer Gerhards and Others``).【F:doc/source/conf.py†L601-L603】

Most existing pages define only description/keyword metadata, so they inherit the
global author. For example, ``doc/source/concepts/log_pipeline/stages.rst`` has a
``.. meta::`` block without an author entry, so the JSON-LD author defaults to
the global value.【F:doc/source/concepts/log_pipeline/stages.rst†L1-L16】 To set a
specific author on a page, add an author entry to its ``.. meta::`` block:

```
.. meta::
   :author: Jane Doe
   :description: Short synopsis for search and JSON-LD.
```

This value will be used for the JSON-LD ``author`` field during HTML builds unless
`DISABLE_JSON_LD` is set.

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
