#!/usr/bin/env python
# Copyright (C) 2010 Google Inc. All rights reserved.
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

"""Compares the text output of a test to the expected text output.

If the output doesn't match, returns FailureTextMismatch and outputs the diff
files into the layout test results directory.
"""

import errno
import logging

from webkitpy.layout_tests.layout_package import test_failures
from webkitpy.layout_tests.test_types import test_type_base

_log = logging.getLogger("webkitpy.layout_tests.test_types.text_diff")


class TestTextDiff(test_type_base.TestTypeBase):

    def _get_normalized_output_text(self, output):
        """Returns the normalized text output, i.e. the output in which
        the end-of-line characters are normalized to "\n"."""
        # Running tests on Windows produces "\r\n".  The "\n" part is helpfully
        # changed to "\r\n" by our system (Python/Cygwin), resulting in
        # "\r\r\n", when, in fact, we wanted to compare the text output with
        # the normalized text expectation files.
        return output.replace("\r\r\n", "\r\n").replace("\r\n", "\n")

    def compare_output(self, port, filename, test_args, actual_test_output,
                        expected_test_output):
        """Implementation of CompareOutput that checks the output text against
        the expected text from the LayoutTest directory."""
        failures = []

        # If we're generating a new baseline, we pass.
        if test_args.new_baseline or test_args.reset_results:
            # Although all test_shell/DumpRenderTree output should be utf-8,
            # we do not ever decode it inside run-webkit-tests.  For some tests
            # DumpRenderTree may not output utf-8 text (e.g. webarchives).
            self._save_baseline_data(filename, actual_test_output.text,
                                     ".txt", encoding=None,
                                     generate_new_baseline=test_args.new_baseline)
            return failures

        # Normalize text to diff
        actual_text = self._get_normalized_output_text(actual_test_output.text)
        # Assuming expected_text is already normalized.
        expected_text = expected_test_output.text

        # Write output files for new tests, too.
        if port.compare_text(actual_text, expected_text):
            # Text doesn't match, write output files.
            self.write_output_files(filename, ".txt", actual_text,
                                    expected_text, encoding=None,
                                    print_text_diffs=True)

            if expected_text == '':
                failures.append(test_failures.FailureMissingResult())
            else:
                failures.append(test_failures.FailureTextMismatch())

        return failures
