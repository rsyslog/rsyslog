# Helper functions for main conf.py Sphinx build configuration

import datetime
import re
import subprocess

def get_current_branch():
    """Return the current branch we are on or the branch that the detached head
    is pointed to"""

    current_branch = subprocess.check_output(
        ['git', 'rev-parse', '--abbrev-ref', "HEAD"]
        ).decode("utf-8").strip()

    if current_branch == 'HEAD':
        # This means we are operating in a detached head state, will need to
        # parse out the branch that the commit is from.

        # Decode "bytes" type to UTF-8 sting to avoid Python 3 error:
        # "TypeError: a bytes-like object is required, not 'str'""
        # https://docs.python.org/3/library/stdtypes.html#bytes.decode
        branches = subprocess.check_output(['git', 'branch']).decode('utf-8').split('\n')
        for branch in branches:

            # Git marks the current branch, or in this case the branch
            # we are currently detached from with an asterisk
            if '*' in branch:
                # Split on the remote/branch separator, grab the
                # last entry in the list and then strip off the trailing
                # parentheis
                detached_from_branch = branch.split('/')[-1].replace(')', '')

                return detached_from_branch

    else:
        # The assumption is that we are on a branch at this point. Return that.
        return current_branch


def get_current_stable_version():
    """Return the current X.Y stable version number from the latest git tag"""

    def get_latest_tag():
        """"Helper function: Return the latest git tag"""

        git_tag_output = subprocess.check_output(
            ['git', 'tag', '--list', "v*"]
            ).decode("utf-8").strip()

        git_tag_list = re.sub('[A-Za-z]', '', git_tag_output).split('\n')
        git_tag_list.sort(key=lambda s: [int(u) for u in s.split('.')])

        # The latest tag is the last in the list
        git_tag_latest = git_tag_list[-1]

        return git_tag_latest

    latest_tag = get_latest_tag()

    # Return 'X.Y' from 'X.Y.Z'
    return latest_tag[:-2]


def get_next_stable_version():
    """Return the next stable version"""

    current_version = get_current_stable_version()

    # Break apart 'x.y' value, increment y and then concatenate into 'x.y' again
    next_version = "{}.{}".format(
        int(current_version[:1]),
        int(current_version[-2:]) + 1
        )

    return next_version


def get_current_commit_hash():
    """Return commit hash string"""

    commit_hash = subprocess.check_output(['git', 'log', '--pretty=format:%h', 'HEAD', '-n1']).decode("utf-8")

    return commit_hash


def get_release_string(release_type, release_string_detail, version):
    """Return a release string representing the type of build. Verbose for
    dev builds and with sparse version info for release builds"""

    if release_type == "dev":

        # Used in dev builds
        DATE = datetime.date.today()
        TODAY = DATE.strftime('%Y%m%d')

        # The detailed release string is too long for the rsyslog.com 'better'
        # theme and perhaps too long for other themes as well, so we set the
        # level to 'simple' by default (to be explicit) and
        # allow overriding by command-line if desired.
        if release_string_detail == "simple":

            # 'rsyslog' prefix is already set via 'project' variable in conf.py
            # HASH
            # 'docs' string suffix is already ...
            release_string = "{}".format(
                get_current_commit_hash()
                )
        elif release_string_detail == "detailed":

            # The verbose variation of the release string. This was previously
            # the default when using the 'classic' theme, but proves to be
            # too long when viewed using alternate themes. If requested
            # via command-line override this format is used.
            release_string = "{}-{}-{}-{}".format(
                get_next_stable_version(),
                get_current_branch(),
                TODAY,
                get_current_commit_hash()
                )
        else:
            # This means that someone set a value that we do not
            # have a format defined for. Return an error string instead
            # to help make it clear what happened.
            release_string = "invalid value for release_string_detail"

    else:
        release_string = "{}".format(version)

    return release_string
