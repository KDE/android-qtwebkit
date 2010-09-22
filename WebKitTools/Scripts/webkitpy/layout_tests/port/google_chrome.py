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


def GetGoogleChromePort(**kwargs):
    """Some tests have slightly different results when compiled as Google
    Chrome vs Chromium.  In those cases, we prepend an additional directory to
    to the baseline paths."""
    port_name = kwargs['port_name']
    del kwargs['port_name']
    if port_name == 'google-chrome-linux32':
        import chromium_linux

        class GoogleChromeLinux32Port(chromium_linux.ChromiumLinuxPort):
            def baseline_search_path(self):
                paths = chromium_linux.ChromiumLinuxPort.baseline_search_path(
                    self)
                paths.insert(0, self._webkit_baseline_path(
                    'google-chrome-linux32'))
                return paths
        return GoogleChromeLinux32Port(**kwargs)
    elif port_name == 'google-chrome-linux64':
        import chromium_linux

        class GoogleChromeLinux64Port(chromium_linux.ChromiumLinuxPort):
            def baseline_search_path(self):
                paths = chromium_linux.ChromiumLinuxPort.baseline_search_path(
                    self)
                paths.insert(0, self._webkit_baseline_path(
                    'google-chrome-linux64'))
                return paths
        return GoogleChromeLinux64Port(**kwargs)
    elif port_name.startswith('google-chrome-mac'):
        import chromium_mac

        class GoogleChromeMacPort(chromium_mac.ChromiumMacPort):
            def baseline_search_path(self):
                paths = chromium_mac.ChromiumMacPort.baseline_search_path(
                    self)
                paths.insert(0, self._webkit_baseline_path(
                    'google-chrome-mac'))
                return paths
        return GoogleChromeMacPort(**kwargs)
    elif port_name.startswith('google-chrome-win'):
        import chromium_win

        class GoogleChromeWinPort(chromium_win.ChromiumWinPort):
            def baseline_search_path(self):
                paths = chromium_win.ChromiumWinPort.baseline_search_path(
                    self)
                paths.insert(0, self._webkit_baseline_path(
                    'google-chrome-win'))
                return paths
        return GoogleChromeWinPort(**kwargs)
    raise NotImplementedError('unsupported port: %s' % port_name)
