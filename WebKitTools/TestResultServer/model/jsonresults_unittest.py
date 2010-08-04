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

import unittest

from jsonresults import JsonResults

JSON_RESULTS_TEMPLATE = (
    '{"Webkit":{'
    '"allFixableCount":[[TESTDATA_COUNT]],'
    '"buildNumbers":[[TESTDATA_BUILDNUMBERS]],'
    '"chromeRevision":[[TESTDATA_CHROMEREVISION]],'
    '"deferredCounts":[[TESTDATA_COUNTS]],'
    '"fixableCount":[[TESTDATA_COUNT]],'
    '"fixableCounts":[[TESTDATA_COUNTS]],'
    '"secondsSinceEpoch":[[TESTDATA_TIMES]],'
    '"tests":{[TESTDATA_TESTS]},'
    '"webkitRevision":[[TESTDATA_WEBKITREVISION]],'
    '"wontfixCounts":[[TESTDATA_COUNTS]]'
    '},'
    '"version":3'
    '}')

JSON_RESULTS_COUNTS_TEMPLATE = (
    '{'
    '"C":[TESTDATA],'
    '"F":[TESTDATA],'
    '"I":[TESTDATA],'
    '"O":[TESTDATA],'
    '"P":[TESTDATA],'
    '"T":[TESTDATA],'
    '"X":[TESTDATA],'
    '"Z":[TESTDATA]}')

JSON_RESULTS_TESTS_TEMPLATE = (
    '"[TESTDATA_TEST_NAME]":{'
    '"results":[[TESTDATA_TEST_RESULTS]],'
    '"times":[[TESTDATA_TEST_TIMES]]}')

JSON_RESULTS_PREFIX = "ADD_RESULTS("
JSON_RESULTS_SUFFIX = ");"


class JsonResultsTest(unittest.TestCase):
    def setUp(self):
        self._builder = "Webkit"

    def _make_test_json(self, test_data):
        if not test_data:
            return JSON_RESULTS_PREFIX + JSON_RESULTS_SUFFIX

        (builds, tests) = test_data
        if not builds or not tests:
            return JSON_RESULTS_PREFIX + JSON_RESULTS_SUFFIX

        json = JSON_RESULTS_TEMPLATE

        counts = []
        build_numbers = []
        webkit_revision = []
        chrome_revision = []
        times = []
        for build in builds:
            counts.append(JSON_RESULTS_COUNTS_TEMPLATE.replace("[TESTDATA]", build))
            build_numbers.append("1000%s" % build)
            webkit_revision.append("2000%s" % build)
            chrome_revision.append("3000%s" % build)
            times.append("100000%s000" % build)

        json = json.replace("[TESTDATA_COUNTS]", ",".join(counts))
        json = json.replace("[TESTDATA_COUNT]", ",".join(builds))
        json = json.replace("[TESTDATA_BUILDNUMBERS]", ",".join(build_numbers))
        json = json.replace("[TESTDATA_WEBKITREVISION]", ",".join(webkit_revision))
        json = json.replace("[TESTDATA_CHROMEREVISION]", ",".join(chrome_revision))
        json = json.replace("[TESTDATA_TIMES]", ",".join(times))

        json_tests = []
        for test in tests:
            t = JSON_RESULTS_TESTS_TEMPLATE.replace("[TESTDATA_TEST_NAME]", test[0])
            t = t.replace("[TESTDATA_TEST_RESULTS]", test[1])
            t = t.replace("[TESTDATA_TEST_TIMES]", test[2])
            json_tests.append(t)

        json = json.replace("[TESTDATA_TESTS]", ",".join(json_tests))

        return JSON_RESULTS_PREFIX + json + JSON_RESULTS_SUFFIX

    def _test_merge(self, aggregated_data, incremental_data, expected_data):
        aggregated_results = self._make_test_json(aggregated_data)
        incremental_results = self._make_test_json(incremental_data)
        merged_results = JsonResults.merge(self._builder,
            aggregated_results, incremental_results, sort_keys=True)

        if expected_data:
            expected_results = self._make_test_json(expected_data)
            self.assertEquals(merged_results, expected_results)
        else:
            self.assertFalse(merged_results)

    def test(self):
        # Empty incremental results json.
        # Nothing to merge.
        self._test_merge(
            # Aggregated results
            (["2", "1"], [["001.html", "[200,\"P\"]", "[200,\"0\"]"]]),
            # Incremental results
            None,
            # Expect no merge happens.
            None)

        # No actual incremental test results (only prefix and suffix) to merge.
        # Nothing to merge.
        self._test_merge(
            # Aggregated results
            (["2", "1"], [["001.html", "[200,\"P\"]", "[200,\"0\"]"]]),
            # Incremental results
            ([], []),
            # Expected no merge happens.
            None)

        # No existing aggregated results.
        # Merged results == new incremental results.
        self._test_merge(
            # Aggregated results
            None,
            # Incremental results
            (["2", "1"], [["001.html", "[200,\"P\"]", "[200,\"0\"]"]]),
            # Expected results
            (["2", "1"], [["001.html", "[200,\"P\"]", "[200,\"0\"]"]]))

        # Single test for single run.
        # Incremental results has the latest build and same test results for
        # that run.
        # Insert the incremental results at the first place and sum number
        # of runs for "P" (200 + 1) to get merged results.
        self._test_merge(
            # Aggregated results
            (["2", "1"], [["001.html", "[200,\"P\"]", "[200,\"0\"]"]]),
            # Incremental results
            (["3"], [["001.html", "[1,\"P\"]", "[1,\"0\"]"]]),
            # Expected results
            (["3", "2", "1"], [["001.html", "[201,\"P\"]", "[201,\"0\"]"]]))

        # Single test for single run.
        # Incremental results has the latest build but different test results
        # for that run.
        # Insert the incremental results at the first place.
        self._test_merge(
            # Aggregated results
            (["2", "1"], [["001.html", "[200,\"P\"]", "[200,\"0\"]"]]),
            # Incremental results
            (["3"], [["001.html", "[1, \"I\"]", "[1,\"1\"]"]]),
            # Expected results
            (["3", "2", "1"], [["001.html", "[1,\"I\"],[200,\"P\"]", "[1,\"1\"],[200,\"0\"]"]]))

        # Single test for single run.
        # Incremental results has the latest build but different test results
        # for that run.
        # The test "results" and "times" need to be continuous, so the old
        # [10,"I"] result should be dropped because a new result of same type [1,"I"]
        # is inserted in front of [200,"P"].
        self._test_merge(
            # Aggregated results
            (["2", "1"], [["001.html", "[200,\"P\"],[10,\"I\"]", "[200,\"0\"],[10,\"1\"]"]]),
            # Incremental results
            (["3"], [["001.html", "[1,\"I\"]", "[1,\"1\"]"]]),
            # Expected results
            (["3", "2", "1"], [["001.html", "[1,\"I\"],[200,\"P\"]", "[1,\"1\"],[200,\"0\"]"]]))

        # Multiple tests for single run.
        # All tests have incremental updates.
        self._test_merge(
            # Aggregated results
            (["2", "1"], [["001.html", "[200,\"P\"]", "[200,\"0\"]"], ["002.html", "[100,\"I\"]", "[100,\"1\"]"]]),
            # Incremental results
            (["3"], [["001.html", "[1,\"P\"]", "[1,\"0\"]"], ["002.html", "[1,\"I\"]", "[1,\"1\"]"]]),
            # Expected results
            (["3", "2", "1"], [["001.html", "[201,\"P\"]", "[201,\"0\"]"], ["002.html", "[101,\"I\"]", "[101,\"1\"]"]]))

        # Multiple tests for single run.
        # Not all tests have update.
        self._test_merge(
            # Aggregated results
            (["2", "1"], [["001.html", "[200,\"P\"]", "[200,\"0\"]"], ["002.html", "[100,\"I\"]", "[100,\"1\"]"]]),
            # Incremental results
            (["3"], [["002.html", "[1,\"I\"]", "[1,\"1\"]"]]),
            # Expected results
            (["3", "2", "1"], [["001.html", "[200,\"P\"]", "[200,\"0\"]"], ["002.html", "[101,\"I\"]", "[101,\"1\"]"]]))

        # Single test for multiple runs.
        self._test_merge(
            # Aggregated results
            (["2", "1"], [["001.html", "[200,\"P\"]", "[200,\"0\"]"]]),
            # Incremental results
            (["4", "3"], [["001.html", "[2, \"I\"]", "[2,\"2\"]"]]),
            # Expected results
            (["4", "3", "2", "1"], [["001.html", "[2,\"I\"],[200,\"P\"]", "[2,\"2\"],[200,\"0\"]"]]))

        # Multiple tests for multiple runs.
        self._test_merge(
            # Aggregated results
            (["2", "1"], [["001.html", "[200,\"P\"]", "[200,\"0\"]"], ["002.html", "[10,\"Z\"]", "[10,\"0\"]"]]),
            # Incremental results
            (["4", "3"], [["001.html", "[2, \"I\"]", "[2,\"2\"]"], ["002.html", "[1,\"C\"]", "[1,\"1\"]"]]),
            # Expected results
            (["4", "3", "2", "1"], [["001.html", "[2,\"I\"],[200,\"P\"]", "[2,\"2\"],[200,\"0\"]"], ["002.html", "[1,\"C\"],[10,\"Z\"]", "[1,\"1\"],[10,\"0\"]"]]))

        # Test the build in incremental results is older than the most recent
        # build in aggregated results.
        # The incremental results should be dropped and no merge happens.
        self._test_merge(
            # Aggregated results
            (["3", "1"], [["001.html", "[200,\"P\"]", "[200,\"0\"]"]]),
            # Incremental results
            (["2"], [["001.html", "[1, \"P\"]", "[1,\"0\"]"]]),
            # Expected no merge happens.
            None)

        # Test the build in incremental results is same as the build in
        # aggregated results.
        # The incremental results should be dropped and no merge happens.
        self._test_merge(
            # Aggregated results
            (["2", "1"], [["001.html", "[200,\"P\"]", "[200,\"0\"]"]]),
            # Incremental results
            (["3", "2"], [["001.html", "[2, \"P\"]", "[2,\"0\"]"]]),
            # Expected no merge happens.
            None)

if __name__ == '__main__':
    unittest.main()
