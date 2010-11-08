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

"""Wrapper objects for WebKit-specific utility routines."""

# FIXME: This file needs to be unified with common/checkout/scm.py and
# common/config/ports.py .

from __future__ import with_statement

import codecs
import os

from webkitpy.common.checkout import scm
from webkitpy.common.system import logutils


_log = logutils.get_logger(__file__)

#
# This is used to record if we've already hit the filesystem to look
# for a default configuration. We cache this to speed up the unit tests,
# but this can be reset with clear_cached_configuration().
#
_determined_configuration = None


def clear_cached_configuration():
    global _determined_configuration
    _determined_configuration = -1


class Config(object):
    _FLAGS_FROM_CONFIGURATIONS = {
        "Debug": "--debug",
        "Release": "--release",
    }

    def __init__(self, executive):
        self._executive = executive
        self._webkit_base_dir = None
        self._default_configuration = None
        self._build_directories = {}

    def build_directory(self, configuration):
        """Returns the path to the build directory for the configuration."""
        if configuration:
            flags = ["--configuration",
                     self._FLAGS_FROM_CONFIGURATIONS[configuration]]
            configuration = ""
        else:
            configuration = ""
            flags = ["--top-level"]

        if not self._build_directories.get(configuration):
            args = ["perl", self._script_path("webkit-build-directory")] + flags
            self._build_directories[configuration] = (
                self._executive.run_command(args).rstrip())

        return self._build_directories[configuration]

    def build_dumprendertree(self, configuration):
        """Builds DRT in the given configuration.

        Returns True if the  build was successful and up-to-date."""
        flag = self._FLAGS_FROM_CONFIGURATIONS[configuration]
        exit_code = self._executive.run_command([
            self._script_path("build-dumprendertree"), flag],
            return_exit_code=True)
        if exit_code != 0:
            _log.error("Failed to build DumpRenderTree")
            return False
        return True

    def default_configuration(self):
        """Returns the default configuration for the user.

        Returns the value set by 'set-webkit-configuration', or "Release"
        if that has not been set. This mirrors the logic in webkitdirs.pm."""
        if not self._default_configuration:
            self._default_configuration = self._determine_configuration()
        if not self._default_configuration:
            self._default_configuration = 'Release'
        if self._default_configuration not in self._FLAGS_FROM_CONFIGURATIONS:
            _log.warn("Configuration \"%s\" is not a recognized value.\n" %
                      self._default_configuration)
            _log.warn("Scripts may fail.  "
                      "See 'set-webkit-configuration --help'.")
        return self._default_configuration

    def path_from_webkit_base(self, *comps):
        return os.path.join(self.webkit_base_dir(), *comps)

    def webkit_base_dir(self):
        """Returns the absolute path to the top of the WebKit tree.

        Raises an AssertionError if the top dir can't be determined."""
        # FIXME: Consider determining this independently of scm in order
        # to be able to run in a bare tree.
        if not self._webkit_base_dir:
            self._webkit_base_dir = scm.find_checkout_root()
            assert self._webkit_base_dir, "Could not determine the top of the WebKit checkout"
        return self._webkit_base_dir

    def _script_path(self, script_name):
        return os.path.join(self.webkit_base_dir(), "WebKitTools",
                            "Scripts", script_name)

    def _determine_configuration(self):
        # This mirrors the logic in webkitdirs.pm:determineConfiguration().
        global _determined_configuration
        if _determined_configuration == -1:
            contents = self._read_configuration()
            if contents == "Deployment":
                contents = "Release"
            if contents == "Development":
                contents = "Debug"
            _determined_configuration = contents
        return _determined_configuration

    def _read_configuration(self):
        configuration_path = os.path.join(self.build_directory(None),
                                          "Configuration")
        if not os.path.exists(configuration_path):
            return None

        with codecs.open(configuration_path, "r", "utf-8") as fh:
            return fh.read().rstrip()
