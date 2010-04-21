# Copyright (c) 2009, Google Inc. All rights reserved.
# Copyright (c) 2009 Apple Inc. All rights reserved.
# 
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
# 
#     * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#     * Neither the name of Google Inc. nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
# 
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#
# Python module for interacting with an SCM system (like SVN or Git)

import os
import re

# FIXME: Instead of using run_command directly, most places in this
# class would rather use an SCM.run method which automatically set
# cwd=self.checkout_root.
from webkitpy.common.system.executive import Executive, run_command, ScriptError
from webkitpy.common.system.user import User
from webkitpy.common.system.deprecated_logging import error, log


def detect_scm_system(path):
    if SVN.in_working_directory(path):
        return SVN(cwd=path)
    
    if Git.in_working_directory(path):
        return Git(cwd=path)
    
    return None


def first_non_empty_line_after_index(lines, index=0):
    first_non_empty_line = index
    for line in lines[index:]:
        if re.match("^\s*$", line):
            first_non_empty_line += 1
        else:
            break
    return first_non_empty_line


class CommitMessage:
    def __init__(self, message):
        self.message_lines = message[first_non_empty_line_after_index(message, 0):]

    def body(self, lstrip=False):
        lines = self.message_lines[first_non_empty_line_after_index(self.message_lines, 1):]
        if lstrip:
            lines = [line.lstrip() for line in lines]
        return "\n".join(lines) + "\n"

    def description(self, lstrip=False, strip_url=False):
        line = self.message_lines[0]
        if lstrip:
            line = line.lstrip()
        if strip_url:
            line = re.sub("^(\s*)<.+> ", "\1", line)
        return line

    def message(self):
        return "\n".join(self.message_lines) + "\n"


class CheckoutNeedsUpdate(ScriptError):
    def __init__(self, script_args, exit_code, output, cwd):
        ScriptError.__init__(self, script_args=script_args, exit_code=exit_code, output=output, cwd=cwd)


def commit_error_handler(error):
    if re.search("resource out of date", error.output):
        raise CheckoutNeedsUpdate(script_args=error.script_args, exit_code=error.exit_code, output=error.output, cwd=error.cwd)
    Executive.default_error_handler(error)


# SCM methods are expected to return paths relative to self.checkout_root.
class SCM:
    def __init__(self, cwd):
        self.cwd = cwd
        self.checkout_root = self.find_checkout_root(self.cwd)
        self.dryrun = False

    # SCM always returns repository relative path, but sometimes we need
    # absolute paths to pass to rm, etc.
    def absolute_path(self, repository_relative_path):
        return os.path.join(self.checkout_root, repository_relative_path)

    # FIXME: This belongs in Checkout, not SCM.
    def scripts_directory(self):
        return os.path.join(self.checkout_root, "WebKitTools", "Scripts")

    # FIXME: This belongs in Checkout, not SCM.
    def script_path(self, script_name):
        return os.path.join(self.scripts_directory(), script_name)

    def ensure_clean_working_directory(self, force_clean):
        if not force_clean and not self.working_directory_is_clean():
            # FIXME: Shouldn't this use cwd=self.checkout_root?
            print run_command(self.status_command(), error_handler=Executive.ignore_error)
            raise ScriptError(message="Working directory has modifications, pass --force-clean or --no-clean to continue.")
        
        log("Cleaning working directory")
        self.clean_working_directory()
    
    def ensure_no_local_commits(self, force):
        if not self.supports_local_commits():
            return
        commits = self.local_commits()
        if not len(commits):
            return
        if not force:
            error("Working directory has local commits, pass --force-clean to continue.")
        self.discard_local_commits()

    def run_status_and_extract_filenames(self, status_command, status_regexp):
        filenames = []
        # We run with cwd=self.checkout_root so that returned-paths are root-relative.
        for line in run_command(status_command, cwd=self.checkout_root).splitlines():
            match = re.search(status_regexp, line)
            if not match:
                continue
            # status = match.group('status')
            filename = match.group('filename')
            filenames.append(filename)
        return filenames

    def strip_r_from_svn_revision(self, svn_revision):
        match = re.match("^r(?P<svn_revision>\d+)", svn_revision)
        if (match):
            return match.group('svn_revision')
        return svn_revision

    def svn_revision_from_commit_text(self, commit_text):
        match = re.search(self.commit_success_regexp(), commit_text, re.MULTILINE)
        return match.group('svn_revision')

    @staticmethod
    def in_working_directory(path):
        raise NotImplementedError, "subclasses must implement"

    @staticmethod
    def find_checkout_root(path):
        raise NotImplementedError, "subclasses must implement"

    @staticmethod
    def commit_success_regexp():
        raise NotImplementedError, "subclasses must implement"

    def working_directory_is_clean(self):
        raise NotImplementedError, "subclasses must implement"

    def clean_working_directory(self):
        raise NotImplementedError, "subclasses must implement"

    def status_command(self):
        raise NotImplementedError, "subclasses must implement"

    def add(self, path):
        raise NotImplementedError, "subclasses must implement"

    def changed_files(self):
        raise NotImplementedError, "subclasses must implement"

    def changed_files_for_revision(self):
        raise NotImplementedError, "subclasses must implement"

    def added_files(self):
        raise NotImplementedError, "subclasses must implement"

    def conflicted_files(self):
        raise NotImplementedError, "subclasses must implement"

    def display_name(self):
        raise NotImplementedError, "subclasses must implement"

    def create_patch(self):
        raise NotImplementedError, "subclasses must implement"

    def committer_email_for_revision(self, revision):
        raise NotImplementedError, "subclasses must implement"

    def contents_at_revision(self, path, revision):
        raise NotImplementedError, "subclasses must implement"

    def diff_for_revision(self, revision):
        raise NotImplementedError, "subclasses must implement"

    def apply_reverse_diff(self, revision):
        raise NotImplementedError, "subclasses must implement"

    def revert_files(self, file_paths):
        raise NotImplementedError, "subclasses must implement"

    def commit_with_message(self, message, username=None):
        raise NotImplementedError, "subclasses must implement"

    def svn_commit_log(self, svn_revision):
        raise NotImplementedError, "subclasses must implement"

    def last_svn_commit_log(self):
        raise NotImplementedError, "subclasses must implement"

    # Subclasses must indicate if they support local commits,
    # but the SCM baseclass will only call local_commits methods when this is true.
    @staticmethod
    def supports_local_commits():
        raise NotImplementedError, "subclasses must implement"

    def svn_merge_base():
        raise NotImplementedError, "subclasses must implement"

    def create_patch_from_local_commit(self, commit_id):
        error("Your source control manager does not support creating a patch from a local commit.")

    def create_patch_since_local_commit(self, commit_id):
        error("Your source control manager does not support creating a patch from a local commit.")

    def commit_locally_with_message(self, message):
        error("Your source control manager does not support local commits.")

    def discard_local_commits(self):
        pass

    def local_commits(self):
        return []


class SVN(SCM):
    # FIXME: We should move these values to a WebKit-specific config. file.
    svn_server_host = "svn.webkit.org"
    svn_server_realm = "<http://svn.webkit.org:80> Mac OS Forge"

    def __init__(self, cwd):
        SCM.__init__(self, cwd)
        self.cached_version = None
    
    @staticmethod
    def in_working_directory(path):
        return os.path.isdir(os.path.join(path, '.svn'))
    
    @classmethod
    def find_uuid(cls, path):
        if not cls.in_working_directory(path):
            return None
        return cls.value_from_svn_info(path, 'Repository UUID')

    @classmethod
    def value_from_svn_info(cls, path, field_name):
        svn_info_args = ['svn', 'info', path]
        info_output = run_command(svn_info_args).rstrip()
        match = re.search("^%s: (?P<value>.+)$" % field_name, info_output, re.MULTILINE)
        if not match:
            raise ScriptError(script_args=svn_info_args, message='svn info did not contain a %s.' % field_name)
        return match.group('value')

    @staticmethod
    def find_checkout_root(path):
        uuid = SVN.find_uuid(path)
        # If |path| is not in a working directory, we're supposed to return |path|.
        if not uuid:
            return path
        # Search up the directory hierarchy until we find a different UUID.
        last_path = None
        while True:
            if uuid != SVN.find_uuid(path):
                return last_path
            last_path = path
            (path, last_component) = os.path.split(path)
            if last_path == path:
                return None

    @staticmethod
    def commit_success_regexp():
        return "^Committed revision (?P<svn_revision>\d+)\.$"

    def has_authorization_for_realm(self, realm=svn_server_realm, home_directory=os.getenv("HOME")):
        # Assumes find and grep are installed.
        if not os.path.isdir(os.path.join(home_directory, ".subversion")):
            return False
        find_args = ["find", ".subversion", "-type", "f", "-exec", "grep", "-q", realm, "{}", ";", "-print"];
        find_output = run_command(find_args, cwd=home_directory, error_handler=Executive.ignore_error).rstrip()
        return find_output and os.path.isfile(os.path.join(home_directory, find_output))

    def svn_version(self):
        if not self.cached_version:
            self.cached_version = run_command(['svn', '--version', '--quiet'])
        
        return self.cached_version

    def working_directory_is_clean(self):
        return run_command(["svn", "diff"], cwd=self.checkout_root) == ""

    def clean_working_directory(self):
        # svn revert -R is not as awesome as git reset --hard.
        # It will leave added files around, causing later svn update
        # calls to fail on the bots.  We make this mirror git reset --hard
        # by deleting any added files as well.
        added_files = reversed(sorted(self.added_files()))
        # added_files() returns directories for SVN, we walk the files in reverse path
        # length order so that we remove files before we try to remove the directories.
        run_command(["svn", "revert", "-R", "."], cwd=self.checkout_root)
        for path in added_files:
            # This is robust against cwd != self.checkout_root
            absolute_path = self.absolute_path(path)
            # Completely lame that there is no easy way to remove both types with one call.
            if os.path.isdir(path):
                os.rmdir(absolute_path)
            else:
                os.remove(absolute_path)

    def status_command(self):
        return ['svn', 'status']

    def _status_regexp(self, expected_types):
        field_count = 6 if self.svn_version() > "1.6" else 5
        return "^(?P<status>[%s]).{%s} (?P<filename>.+)$" % (expected_types, field_count)

    def add(self, path):
        # path is assumed to be cwd relative?
        run_command(["svn", "add", path])

    def changed_files(self):
        return self.run_status_and_extract_filenames(self.status_command(), self._status_regexp("ACDMR"))

    def changed_files_for_revision(self, revision):
        # As far as I can tell svn diff --summarize output looks just like svn status output.
        status_command = ["svn", "diff", "--summarize", "-c", str(revision)]
        return self.run_status_and_extract_filenames(status_command, self._status_regexp("ACDMR"))

    def conflicted_files(self):
        return self.run_status_and_extract_filenames(self.status_command(), self._status_regexp("C"))

    def added_files(self):
        return self.run_status_and_extract_filenames(self.status_command(), self._status_regexp("A"))

    @staticmethod
    def supports_local_commits():
        return False

    def display_name(self):
        return "svn"

    def create_patch(self):
        return run_command(self.script_path("svn-create-patch"), cwd=self.checkout_root, return_stderr=False)

    def committer_email_for_revision(self, revision):
        return run_command(["svn", "propget", "svn:author", "--revprop", "-r", str(revision)]).rstrip()

    def contents_at_revision(self, path, revision):
        remote_path = "%s/%s" % (self._repository_url(), path)
        return run_command(["svn", "cat", "-r", str(revision), remote_path])

    def diff_for_revision(self, revision):
        # FIXME: This should probably use cwd=self.checkout_root
        return run_command(['svn', 'diff', '-c', str(revision)])

    def _repository_url(self):
        return self.value_from_svn_info(self.checkout_root, 'URL')

    def apply_reverse_diff(self, revision):
        # '-c -revision' applies the inverse diff of 'revision'
        svn_merge_args = ['svn', 'merge', '--non-interactive', '-c', '-%s' % revision, self._repository_url()]
        log("WARNING: svn merge has been known to take more than 10 minutes to complete.  It is recommended you use git for rollouts.")
        log("Running '%s'" % " ".join(svn_merge_args))
        # FIXME: Should this use cwd=self.checkout_root?
        run_command(svn_merge_args)

    def revert_files(self, file_paths):
        # FIXME: This should probably use cwd=self.checkout_root.
        run_command(['svn', 'revert'] + file_paths)

    def commit_with_message(self, message, username=None):
        if self.dryrun:
            # Return a string which looks like a commit so that things which parse this output will succeed.
            return "Dry run, no commit.\nCommitted revision 0."
        svn_commit_args = ["svn", "commit"]
        if not username and not self.has_authorization_for_realm():
            username = User.prompt("%s login: " % self.svn_server_host, repeat=5)
            if not username:
                raise Exception("You need to specify the username on %s to perform the commit as." % self.svn_server_host)
        if username:
            svn_commit_args.extend(["--username", username])
        svn_commit_args.extend(["-m", message])
        # FIXME: Should this use cwd=self.checkout_root?
        return run_command(svn_commit_args, error_handler=commit_error_handler)

    def svn_commit_log(self, svn_revision):
        svn_revision = self.strip_r_from_svn_revision(str(svn_revision))
        return run_command(['svn', 'log', '--non-interactive', '--revision', svn_revision]);

    def last_svn_commit_log(self):
        # BASE is the checkout revision, HEAD is the remote repository revision
        # http://svnbook.red-bean.com/en/1.0/ch03s03.html
        return self.svn_commit_log('BASE')

# All git-specific logic should go here.
class Git(SCM):
    def __init__(self, cwd):
        SCM.__init__(self, cwd)

    @classmethod
    def in_working_directory(cls, path):
        return run_command(['git', 'rev-parse', '--is-inside-work-tree'], cwd=path, error_handler=Executive.ignore_error).rstrip() == "true"

    @classmethod
    def find_checkout_root(cls, path):
        # "git rev-parse --show-cdup" would be another way to get to the root
        (checkout_root, dot_git) = os.path.split(run_command(['git', 'rev-parse', '--git-dir'], cwd=path))
        # If we were using 2.6 # checkout_root = os.path.relpath(checkout_root, path)
        if not os.path.isabs(checkout_root): # Sometimes git returns relative paths
            checkout_root = os.path.join(path, checkout_root)
        return checkout_root

    @classmethod
    def read_git_config(cls, key):
        # FIXME: This should probably use cwd=self.checkout_root.
        return run_command(["git", "config", key],
            error_handler=Executive.ignore_error).rstrip('\n')

    @staticmethod
    def commit_success_regexp():
        return "^Committed r(?P<svn_revision>\d+)$"

    def discard_local_commits(self):
        # FIXME: This should probably use cwd=self.checkout_root
        run_command(['git', 'reset', '--hard', self.svn_branch_name()])
    
    def local_commits(self):
        # FIXME: This should probably use cwd=self.checkout_root
        return run_command(['git', 'log', '--pretty=oneline', 'HEAD...' + self.svn_branch_name()]).splitlines()

    def rebase_in_progress(self):
        return os.path.exists(os.path.join(self.checkout_root, '.git/rebase-apply'))

    def working_directory_is_clean(self):
        # FIXME: This should probably use cwd=self.checkout_root
        return run_command(['git', 'diff', 'HEAD', '--name-only']) == ""

    def clean_working_directory(self):
        # FIXME: These should probably use cwd=self.checkout_root.
        # Could run git clean here too, but that wouldn't match working_directory_is_clean
        run_command(['git', 'reset', '--hard', 'HEAD'])
        # Aborting rebase even though this does not match working_directory_is_clean
        if self.rebase_in_progress():
            run_command(['git', 'rebase', '--abort'])

    def status_command(self):
        # git status returns non-zero when there are changes, so we use git diff name --name-status HEAD instead.
        return ["git", "diff", "--name-status", "HEAD"]

    def _status_regexp(self, expected_types):
        return '^(?P<status>[%s])\t(?P<filename>.+)$' % expected_types

    def add(self, path):
        # path is assumed to be cwd relative?
        run_command(["git", "add", path])

    def changed_files(self):
        status_command = ['git', 'diff', '-r', '--name-status', '-C', '-M', 'HEAD']
        return self.run_status_and_extract_filenames(status_command, self._status_regexp("ADM"))

    def _changes_files_for_commit(self, git_commit):
        # --pretty="format:" makes git show not print the commit log header,
        changed_files = run_command(["git", "show", "--pretty=format:", "--name-only", git_commit]).splitlines()
        # instead it just prints a blank line at the top, so we skip the blank line:
        return changed_files[1:]

    def changed_files_for_revision(self, revision):
        commit_id = self.git_commit_from_svn_revision(revision)
        return self._changes_files_for_commit(commit_id)

    def conflicted_files(self):
        status_command = ['git', 'diff', '--name-status', '-C', '-M', '--diff-filter=U']
        return self.run_status_and_extract_filenames(status_command, self._status_regexp("U"))

    def added_files(self):
        return self.run_status_and_extract_filenames(self.status_command(), self._status_regexp("A"))

    @staticmethod
    def supports_local_commits():
        return True

    def display_name(self):
        return "git"

    def create_patch(self):
        # FIXME: This should probably use cwd=self.checkout_root
        return run_command(['git', 'diff', '--binary', 'HEAD'])

    @classmethod
    def git_commit_from_svn_revision(cls, revision):
        # FIXME: This should probably use cwd=self.checkout_root
        git_commit = run_command(['git', 'svn', 'find-rev', 'r%s' % revision]).rstrip()
        # git svn find-rev always exits 0, even when the revision is not found.
        if not git_commit:
            raise ScriptError(message='Failed to find git commit for revision %s, your checkout likely needs an update.' % revision)
        return git_commit

    def contents_at_revision(self, path, revision):
        return run_command(["git", "show", "%s:%s" % (self.git_commit_from_svn_revision(revision), path)])

    def diff_for_revision(self, revision):
        git_commit = self.git_commit_from_svn_revision(revision)
        return self.create_patch_from_local_commit(git_commit)

    def committer_email_for_revision(self, revision):
        git_commit = self.git_commit_from_svn_revision(revision)
        committer_email = run_command(["git", "log", "-1", "--pretty=format:%ce", git_commit])
        # Git adds an extra @repository_hash to the end of every committer email, remove it:
        return committer_email.rsplit("@", 1)[0]

    def apply_reverse_diff(self, revision):
        # Assume the revision is an svn revision.
        git_commit = self.git_commit_from_svn_revision(revision)
        # I think this will always fail due to ChangeLogs.
        run_command(['git', 'revert', '--no-commit', git_commit], error_handler=Executive.ignore_error)

    def revert_files(self, file_paths):
        run_command(['git', 'checkout', 'HEAD'] + file_paths)

    def commit_with_message(self, message, username=None):
        # Username is ignored during Git commits.
        self.commit_locally_with_message(message)
        return self.push_local_commits_to_server()

    def svn_commit_log(self, svn_revision):
        svn_revision = self.strip_r_from_svn_revision(svn_revision)
        return run_command(['git', 'svn', 'log', '-r', svn_revision])

    def last_svn_commit_log(self):
        return run_command(['git', 'svn', 'log', '--limit=1'])

    # Git-specific methods:

    def delete_branch(self, branch):
        if run_command(['git', 'show-ref', '--quiet', '--verify', 'refs/heads/' + branch], return_exit_code=True) == 0:
            run_command(['git', 'branch', '-D', branch])

    def svn_merge_base(self):
        return run_command(['git', 'merge-base', self.svn_branch_name(), 'HEAD']).strip()

    def svn_branch_name(self):
        return Git.read_git_config('svn-remote.svn.fetch').split(':')[1]

    def create_patch_from_local_commit(self, commit_id):
        return run_command(['git', 'diff', '--binary', commit_id + "^.." + commit_id])

    def create_patch_since_local_commit(self, commit_id):
        return run_command(['git', 'diff', '--binary', commit_id])

    def commit_locally_with_message(self, message):
        run_command(['git', 'commit', '--all', '-F', '-'], input=message)

    def push_local_commits_to_server(self):
        dcommit_command = ['git', 'svn', 'dcommit']
        if self.dryrun:
            dcommit_command.append('--dry-run')
        output = run_command(dcommit_command, error_handler=commit_error_handler)
        # Return a string which looks like a commit so that things which parse this output will succeed.
        if self.dryrun:
            output += "\nCommitted r0"
        return output

    # This function supports the following argument formats:
    # no args : rev-list trunk..HEAD
    # A..B    : rev-list A..B
    # A...B   : error!
    # A B     : [A, B]  (different from git diff, which would use "rev-list A..B")
    def commit_ids_from_commitish_arguments(self, args):
        if not len(args):
            args.append('%s..HEAD' % self.svn_branch_name())

        commit_ids = []
        for commitish in args:
            if '...' in commitish:
                raise ScriptError(message="'...' is not supported (found in '%s'). Did you mean '..'?" % commitish)
            elif '..' in commitish:
                commit_ids += reversed(run_command(['git', 'rev-list', commitish]).splitlines())
            else:
                # Turn single commits or branch or tag names into commit ids.
                commit_ids += run_command(['git', 'rev-parse', '--revs-only', commitish]).splitlines()
        return commit_ids

    def commit_message_for_local_commit(self, commit_id):
        commit_lines = run_command(['git', 'cat-file', 'commit', commit_id]).splitlines()

        # Skip the git headers.
        first_line_after_headers = 0
        for line in commit_lines:
            first_line_after_headers += 1
            if line == "":
                break
        return CommitMessage(commit_lines[first_line_after_headers:])

    def files_changed_summary_for_commit(self, commit_id):
        return run_command(['git', 'diff-tree', '--shortstat', '--no-commit-id', commit_id])
