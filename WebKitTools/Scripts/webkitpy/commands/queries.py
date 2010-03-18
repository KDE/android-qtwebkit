#!/usr/bin/env python
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


from optparse import make_option
import StringIO

from webkitpy.buildbot import BuildBot
from webkitpy.bugzilla import parse_bug_id
from webkitpy.changelogs import ChangeLog, is_path_to_changelog, view_source_url
from webkitpy.committers import CommitterList
from webkitpy.grammar import pluralize
from webkitpy.webkit_logging import log
from webkitpy.multicommandtool import AbstractDeclarativeCommand


class BugsToCommit(AbstractDeclarativeCommand):
    name = "bugs-to-commit"
    help_text = "List bugs in the commit-queue"

    def execute(self, options, args, tool):
        # FIXME: This command is poorly named.  It's fetching the commit-queue list here.  The name implies it's fetching pending-commit (all r+'d patches).
        bug_ids = tool.bugs.queries.fetch_bug_ids_from_commit_queue()
        for bug_id in bug_ids:
            print "%s" % bug_id


class PatchesInCommitQueue(AbstractDeclarativeCommand):
    name = "patches-in-commit-queue"
    help_text = "List patches in the commit-queue"

    def execute(self, options, args, tool):
        patches = tool.bugs.queries.fetch_patches_from_commit_queue()
        log("Patches in commit queue:")
        for patch in patches:
            print patch.url()


class PatchesToCommitQueue(AbstractDeclarativeCommand):
    name = "patches-to-commit-queue"
    help_text = "Patches which should be added to the commit queue"
    def __init__(self):
        options = [
            make_option("--bugs", action="store_true", dest="bugs", help="Output bug links instead of patch links"),
        ]
        AbstractDeclarativeCommand.__init__(self, options=options)

    @staticmethod
    def _needs_commit_queue(patch):
        if patch.commit_queue() == "+": # If it's already cq+, ignore the patch.
            log("%s already has cq=%s" % (patch.id(), patch.commit_queue()))
            return False

        # We only need to worry about patches from contributers who are not yet committers.
        committer_record = CommitterList().committer_by_email(patch.attacher_email())
        if committer_record:
            log("%s committer = %s" % (patch.id(), committer_record))
        return not committer_record

    def execute(self, options, args, tool):
        patches = tool.bugs.queries.fetch_patches_from_pending_commit_list()
        patches_needing_cq = filter(self._needs_commit_queue, patches)
        if options.bugs:
            bugs_needing_cq = map(lambda patch: patch.bug_id(), patches_needing_cq)
            bugs_needing_cq = sorted(set(bugs_needing_cq))
            for bug_id in bugs_needing_cq:
                print "%s" % tool.bugs.bug_url_for_bug_id(bug_id)
        else:
            for patch in patches_needing_cq:
                print "%s" % tool.bugs.attachment_url_for_id(patch.id(), action="edit")


class PatchesToReview(AbstractDeclarativeCommand):
    name = "patches-to-review"
    help_text = "List patches that are pending review"

    def execute(self, options, args, tool):
        patch_ids = tool.bugs.queries.fetch_attachment_ids_from_review_queue()
        log("Patches pending review:")
        for patch_id in patch_ids:
            print patch_id

class WhatBroke(AbstractDeclarativeCommand):
    name = "what-broke"
    help_text = "Print the status of the %s buildbots" % BuildBot.default_host
    long_help = """Fetches build status from http://build.webkit.org/one_box_per_builder
and displayes the status of each builder."""

    def _longest_builder_name(self, builders):
        return max(map(lambda builder: len(builder["name"]), builders))

    # FIXME: This should move onto buildbot.Builder
    def _find_green_to_red_transition(self, builder_status, look_back_limit=30):
        # walk backwards until we find a green build
        builder = self.tool.buildbot.builder_with_name(builder_status["name"])
        red_build = builder.build(builder_status["build_number"])
        green_build = None
        look_back_count = 0
        while True:
            if look_back_count >= look_back_limit:
                break
            # Use a previous_build() method to avoid assuming build numbers are sequential.
            before_red_build = red_build.previous_build()
            if not before_red_build:
                break
            if before_red_build.is_green():
                green_build = before_red_build
                break
            red_build = before_red_build
            look_back_count += 1
        return (green_build, red_build)

    def _print_builder_line(self, builder_name, max_name_width, status_message):
        print "%s : %s" % (builder_name.ljust(max_name_width), status_message)

    def _changelog_entries_for_revision(self, revision):
        changed_files = self.tool.scm().changed_files_for_revision(revision)
        changelog_paths = [path for path in changed_files if is_path_to_changelog(path)]
        changelog_entries = []
        for changelog_path in changelog_paths:
            changelog_contents = self.tool.scm().contents_at_revision(changelog_path, revision)
            changelog_file = StringIO.StringIO(changelog_contents)
            changelog_entry = ChangeLog.parse_latest_entry_from_file(changelog_file)
            changelog_entries.append(changelog_entry)
        return changelog_entries

    def _commit_info_for_revision(self, revision):
        committer_email = self.tool.scm().committer_email_for_revision(revision)
        changelog_entries = self._changelog_entries_for_revision(revision)
        # Assume for now that the first entry has everything we need:
        changelog_entry = changelog_entries[0]
        # FIXME: The commit info dictionary here should probably be its own class?
        return {
            "bug_id" : parse_bug_id(changelog_entry.contents()),
            "revision" : revision,
            "author_name" : changelog_entry.author_name(),
            "author_email" : changelog_entry.author_email(),
            "reviewer_text" : changelog_entry.reviewer_text(), # FIXME: Eventualy we should return an object here.
            "committer_email" : committer_email,
            "committer" : CommitterList().committer_by_email(committer_email) if committer_email else None
        }

    def _print_blame_information_for_commit(self, commit_info):
        print "r%s:" % commit_info["revision"]
        print "  %s" % view_source_url(commit_info["revision"])
        print "  Bug: %s (%s)" % (commit_info["bug_id"], self.tool.bugs.bug_url_for_bug_id(commit_info["bug_id"]))
        print "  Author: %s <%s>" % (commit_info["author_name"], commit_info["author_email"])
        print "  Reviewer: %s" % commit_info["reviewer_text"]
        print "  Committer: %s" % commit_info["committer"]

    def _print_blame_information_for_builder(self, builder_status, name_width):
        (last_green_build, first_red_build) = self._find_green_to_red_transition(builder_status)
        if not last_green_build:
            self._print_builder_line(builder_status["name"], name_width, "FAIL (blame-list: sometime before %s?)" % first_red_build.revision())
            return

        suspect_revisions = range(first_red_build.revision(), last_green_build.revision(), -1)
        suspect_revisions.reverse()
        # FIXME: Parse reviewer and committer from red checkin
        self._print_builder_line(builder_status["name"], name_width, "FAIL (blame-list: %s)" % suspect_revisions)
        for revision in suspect_revisions:
            commit_info = self._commit_info_for_revision(revision)
            self._print_blame_information_for_commit(commit_info)

    def execute(self, options, args, tool):
        builder_statuses = tool.buildbot.builder_statuses()
        longest_builder_name = self._longest_builder_name(builder_statuses)
        failing_builders = 0
        for builder_status in builder_statuses:
            # If the builder is green, print OK, exit.
            if builder_status["is_green"]:
                continue
            self._print_blame_information_for_builder(builder_status, name_width=longest_builder_name)
            failing_builders += 1
        if failing_builders:
            print "%s of %s are failing" % (failing_builders, pluralize("builder", len(builder_statuses)))
        else:
            print "All builders are passing!"


class TreeStatus(AbstractDeclarativeCommand):
    name = "tree-status"
    help_text = "Print the status of the %s buildbots" % BuildBot.default_host
    long_help = """Fetches build status from http://build.webkit.org/one_box_per_builder
and displayes the status of each builder."""

    def execute(self, options, args, tool):
        for builder in tool.buildbot.builder_statuses():
            status_string = "ok" if builder["is_green"] else "FAIL"
            print "%s : %s" % (status_string.ljust(4), builder["name"])
