# Copyright (C) 2010 Chris Jerdonek (cjerdonek@webkit.org)
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1.  Redistributions of source code must retain the above copyright
#     notice, this list of conditions and the following disclaimer.
# 2.  Redistributions in binary form must reproduce the above copyright
#     notice, this list of conditions and the following disclaimer in the
#     documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
# EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
# DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
# (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
# LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
# ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
# SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

"""Unit tests for parser.py."""

import unittest

from optparser import _create_usage
from optparser import ArgumentParser
from optparser import ArgumentPrinter
from optparser import CommandOptionValues as ProcessorOptions
from optparser import DefaultCommandOptionValues


class CreateUsageTest(unittest.TestCase):

    """Tests the _create_usage() function."""

    def test_create_usage(self):
        default_options = DefaultCommandOptionValues(output_format="vs7",
                                                     verbosity=3)
        # Exercise the code path to make sure the function does not error out.
        _create_usage(default_options)


class ArgumentPrinterTest(unittest.TestCase):

    """Tests the ArgumentPrinter class."""

    _printer = ArgumentPrinter()

    def _create_options(self,
                        output_format='emacs',
                        verbosity=3,
                        filter_rules=[],
                        git_commit=None,
                        extra_flag_values={}):
        return ProcessorOptions(extra_flag_values=extra_flag_values,
                                filter_rules=filter_rules,
                                git_commit=git_commit,
                                output_format=output_format,
                                verbosity=verbosity)

    def test_to_flag_string(self):
        options = self._create_options('vs7', 5, ['+foo', '-bar'], 'git',
                                       {'a': 0, 'z': 1})
        self.assertEquals('--a=0 --filter=+foo,-bar --git-commit=git '
                          '--output=vs7 --verbose=5 --z=1',
                          self._printer.to_flag_string(options))

        # This is to check that --filter and --git-commit do not
        # show up when not user-specified.
        options = self._create_options()
        self.assertEquals('--output=emacs --verbose=3',
                          self._printer.to_flag_string(options))


class ArgumentParserTest(unittest.TestCase):

    """Test the ArgumentParser class."""

    def _parse(self):
        """Return a default parse() function for testing."""
        return self._create_parser().parse

    def _create_defaults(self):
        """Return a DefaultCommandOptionValues instance for testing."""
        base_filter_rules = ["-", "+whitespace"]
        return DefaultCommandOptionValues(output_format="vs7",
                                          verbosity=3)

    def _create_parser(self):
        """Return an ArgumentParser instance for testing."""
        def stderr_write(message):
            # We do not want the usage string or style categories
            # to print during unit tests, so print nothing.
            return

        default_options = self._create_defaults()

        all_categories = ["build" ,"whitespace"]
        return ArgumentParser(all_categories=all_categories,
                              base_filter_rules=[],
                              default_options=default_options,
                              stderr_write=stderr_write)

    def test_parse_documentation(self):
        parse = self._parse()

        # FIXME: Test both the printing of the usage string and the
        #        filter categories help.

        # Request the usage string.
        self.assertRaises(SystemExit, parse, ['--help'])
        # Request default filter rules and available style categories.
        self.assertRaises(SystemExit, parse, ['--filter='])

    def test_parse_bad_values(self):
        parse = self._parse()

        # Pass an unsupported argument.
        self.assertRaises(SystemExit, parse, ['--bad'])

        self.assertRaises(ValueError, parse, ['--verbose=bad'])
        self.assertRaises(ValueError, parse, ['--verbose=0'])
        self.assertRaises(ValueError, parse, ['--verbose=6'])
        parse(['--verbose=1']) # works
        parse(['--verbose=5']) # works

        self.assertRaises(ValueError, parse, ['--output=bad'])
        parse(['--output=vs7']) # works

        # Pass a filter rule not beginning with + or -.
        self.assertRaises(ValueError, parse, ['--filter=build'])
        parse(['--filter=+build']) # works
        # Pass files and git-commit at the same time.
        self.assertRaises(SystemExit, parse, ['--git-commit=', 'file.txt'])
        # Pass an extra flag already supported.
        self.assertRaises(ValueError, parse, [], ['filter='])
        parse([], ['extra=']) # works
        # Pass an extra flag with typo.
        self.assertRaises(SystemExit, parse, ['--extratypo='], ['extra='])
        parse(['--extra='], ['extra=']) # works
        self.assertRaises(ValueError, parse, [], ['extra=', 'extra='])


    def test_parse_default_arguments(self):
        parse = self._parse()

        (files, options) = parse([])

        self.assertEquals(files, [])

        self.assertEquals(options.output_format, 'vs7')
        self.assertEquals(options.verbosity, 3)
        self.assertEquals(options.filter_rules, [])
        self.assertEquals(options.git_commit, None)

    def test_parse_explicit_arguments(self):
        parse = self._parse()

        # Pass non-default explicit values.
        (files, options) = parse(['--output=emacs'])
        self.assertEquals(options.output_format, 'emacs')
        (files, options) = parse(['--verbose=4'])
        self.assertEquals(options.verbosity, 4)
        (files, options) = parse(['--git-commit=commit'])
        self.assertEquals(options.git_commit, 'commit')

        # Pass user_rules.
        (files, options) = parse(['--filter=+build,-whitespace'])
        self.assertEquals(options.filter_rules,
                          ["+build", "-whitespace"])

        # Pass spurious white space in user rules.
        (files, options) = parse(['--filter=+build, -whitespace'])
        self.assertEquals(options.filter_rules,
                          ["+build", "-whitespace"])

        # Pass extra flag values.
        (files, options) = parse(['--extra'], ['extra'])
        self.assertEquals(options.extra_flag_values, {'--extra': ''})
        (files, options) = parse(['--extra='], ['extra='])
        self.assertEquals(options.extra_flag_values, {'--extra': ''})
        (files, options) = parse(['--extra=x'], ['extra='])
        self.assertEquals(options.extra_flag_values, {'--extra': 'x'})

    def test_parse_files(self):
        parse = self._parse()

        (files, options) = parse(['foo.cpp'])
        self.assertEquals(files, ['foo.cpp'])

        # Pass multiple files.
        (files, options) = parse(['--output=emacs', 'foo.cpp', 'bar.cpp'])
        self.assertEquals(files, ['foo.cpp', 'bar.cpp'])


class CommandOptionValuesTest(unittest.TestCase):

    """Tests CommandOptionValues class."""

    def test_init(self):
        """Test __init__ constructor."""
        # Check default parameters.
        options = ProcessorOptions()
        self.assertEquals(options.extra_flag_values, {})
        self.assertEquals(options.filter_rules, [])
        self.assertEquals(options.git_commit, None)
        self.assertEquals(options.output_format, "emacs")
        self.assertEquals(options.verbosity, 1)

        # Check argument validation.
        self.assertRaises(ValueError, ProcessorOptions, output_format="bad")
        ProcessorOptions(output_format="emacs") # No ValueError: works
        ProcessorOptions(output_format="vs7") # works
        self.assertRaises(ValueError, ProcessorOptions, verbosity=0)
        self.assertRaises(ValueError, ProcessorOptions, verbosity=6)
        ProcessorOptions(verbosity=1) # works
        ProcessorOptions(verbosity=5) # works

        # Check attributes.
        options = ProcessorOptions(extra_flag_values={"extra_value" : 2},
                                   filter_rules=["+"],
                                   git_commit="commit",
                                   output_format="vs7",
                                   verbosity=3)
        self.assertEquals(options.extra_flag_values, {"extra_value" : 2})
        self.assertEquals(options.filter_rules, ["+"])
        self.assertEquals(options.git_commit, "commit")
        self.assertEquals(options.output_format, "vs7")
        self.assertEquals(options.verbosity, 3)

    def test_eq(self):
        """Test __eq__ equality function."""
        # == calls __eq__.
        self.assertTrue(ProcessorOptions() == ProcessorOptions())

        # Verify that a difference in any argument causes equality to fail.
        options = ProcessorOptions(extra_flag_values={"extra_value" : 1},
                                   filter_rules=["+"],
                                   git_commit="commit",
                                   output_format="vs7",
                                   verbosity=1)
        self.assertFalse(options == ProcessorOptions(extra_flag_values=
                                                     {"extra_value" : 2}))
        self.assertFalse(options == ProcessorOptions(filter_rules=["-"]))
        self.assertFalse(options == ProcessorOptions(git_commit="commit2"))
        self.assertFalse(options == ProcessorOptions(output_format="emacs"))
        self.assertFalse(options == ProcessorOptions(verbosity=2))

    def test_ne(self):
        """Test __ne__ inequality function."""
        # != calls __ne__.
        # By default, __ne__ always returns true on different objects.
        # Thus, just check the distinguishing case to verify that the
        # code defines __ne__.
        self.assertFalse(ProcessorOptions() != ProcessorOptions())

