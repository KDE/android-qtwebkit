# Copyright (C) 2009 Google Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#    * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#    * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#    * Neither the name of Google Inc. nor the names of its
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

import os

from webkitpy.tool.commands_references import Mock
from webkitpy.tool.commands.earlywarningsystem import *
from webkitpy.tool.commands.queuestest import QueuesTest

class EarlyWarningSytemTest(QueuesTest):
    def test_failed_builds(self):
        ews = ChromiumLinuxEWS()
        ews._build = lambda patch, first_run=False: False
        ews._can_build = lambda: True
        ews.review_patch(Mock())

    def test_chromium_linux_ews(self):
        expected_stderr = {
            "begin_work_queue": "CAUTION: chromium-ews will discard all local changes in \"%s\"\nRunning WebKit chromium-ews.\n" % os.getcwd(),
            "handle_unexpected_error": "Mock error message\n",
        }
        self.assert_queue_outputs(ChromiumLinuxEWS(), expected_stderr=expected_stderr)

    def test_chromium_windows_ews(self):
        expected_stderr = {
            "begin_work_queue": "CAUTION: cr-win-ews will discard all local changes in \"%s\"\nRunning WebKit cr-win-ews.\n" % os.getcwd(),
            "handle_unexpected_error": "Mock error message\n",
        }
        self.assert_queue_outputs(ChromiumWindowsEWS(), expected_stderr=expected_stderr)

    def test_qt_ews(self):
        expected_stderr = {
            "begin_work_queue": "CAUTION: qt-ews will discard all local changes in \"%s\"\nRunning WebKit qt-ews.\n" % os.getcwd(),
            "handle_unexpected_error": "Mock error message\n",
        }
        self.assert_queue_outputs(QtEWS(), expected_stderr=expected_stderr)

    def test_gtk_ews(self):
        expected_stderr = {
            "begin_work_queue": "CAUTION: gtk-ews will discard all local changes in \"%s\"\nRunning WebKit gtk-ews.\n" % os.getcwd(),
            "handle_unexpected_error": "Mock error message\n",
        }
        self.assert_queue_outputs(GtkEWS(), expected_stderr=expected_stderr)

    def test_mac_ews(self):
        expected_stderr = {
            "begin_work_queue": "CAUTION: mac-ews will discard all local changes in \"%s\"\nRunning WebKit mac-ews.\n" % os.getcwd(),
            "handle_unexpected_error": "Mock error message\n",
        }
        self.assert_queue_outputs(MacEWS(), expected_stderr=expected_stderr)
