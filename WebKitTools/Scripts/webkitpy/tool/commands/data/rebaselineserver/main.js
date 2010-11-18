/*
 * Copyright (c) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

var ALL_DIRECTORY_PATH = '[all]';

var results;
var testsByFailureType = {};
var testsByDirectory = {};
var selectedTests = [];
var loupe;

function main()
{
    $('failure-type-selector').addEventListener('change', selectFailureType);
    $('directory-selector').addEventListener('change', selectDirectory);
    $('test-selector').addEventListener('change', selectTest);
    $('next-test').addEventListener('click', nextTest);
    $('previous-test').addEventListener('click', previousTest);

    loupe = new Loupe();

    document.addEventListener('keydown', function(event) {
        if (event.altKey || event.ctrlKey || event.metaKey || event.shiftKey) {
            return;
        }

        switch (event.keyIdentifier) {
        case 'Left':
            event.preventDefault();
            previousTest();
            break;
        case 'Right':
            event.preventDefault();
            nextTest();
            break;
        }
    });

    loadText('/results.json', function(text) {
        results = JSON.parse(text);
        displayResults();
    });
}

/**
 * Groups test results by failure type.
 */
function displayResults()
{
    var failureTypeSelector = $('failure-type-selector');
    var failureTypes = [];

    for (var testName in results.tests) {
        var test = results.tests[testName];
        if (test.actual == 'PASS') {
            continue;
        }
        var failureType = test.actual + ' (expected ' + test.expected + ')';
        if (!(failureType in testsByFailureType)) {
            testsByFailureType[failureType] = [];
            failureTypes.push(failureType);
        }
        testsByFailureType[failureType].push(testName);
    }

    // Sort by number of failures
    failureTypes.sort(function(a, b) {
        return testsByFailureType[b].length - testsByFailureType[a].length;
    });

    for (var i = 0, failureType; failureType = failureTypes[i]; i++) {
        var failureTypeOption = document.createElement('option');
        failureTypeOption.value = failureType;
        failureTypeOption.textContent = failureType + ' - ' + testsByFailureType[failureType].length + ' tests';
        failureTypeSelector.appendChild(failureTypeOption);
    }

    selectFailureType();

    document.body.classList.remove('loading');
}

/**
 * For a given failure type, gets all the tests and groups them by directory
 * (populating the directory selector with them).
 */
function selectFailureType()
{
    var selectedFailureType = getSelectValue('failure-type-selector');
    var tests = testsByFailureType[selectedFailureType];

    testsByDirectory = {}
    var displayDirectoryNamesByDirectory = {};
    var directories = [];

    // Include a special option for all tests
    testsByDirectory[ALL_DIRECTORY_PATH] = tests;
    displayDirectoryNamesByDirectory[ALL_DIRECTORY_PATH] = 'all';
    directories.push(ALL_DIRECTORY_PATH);

    // Roll up tests by ancestor directories
    tests.forEach(function(test) {
        var pathPieces = test.split('/');
        var pathDirectories = pathPieces.slice(0, pathPieces.length -1);
        var ancestorDirectory = '';

        pathDirectories.forEach(function(pathDirectory, index) {
            ancestorDirectory += pathDirectory + '/';
            if (!(ancestorDirectory in testsByDirectory)) {
                testsByDirectory[ancestorDirectory] = [];
                var displayDirectoryName = new Array(index * 6).join('&nbsp;') + pathDirectory;
                displayDirectoryNamesByDirectory[ancestorDirectory] = displayDirectoryName;
                directories.push(ancestorDirectory);
            }

            testsByDirectory[ancestorDirectory].push(test);
        });
    });

    directories.sort();

    var directorySelector = $('directory-selector');
    directorySelector.innerHTML = '';

    directories.forEach(function(directory) {
        var directoryOption = document.createElement('option');
        directoryOption.value = directory;
        directoryOption.innerHTML =
            displayDirectoryNamesByDirectory[directory] + ' - ' +
            testsByDirectory[directory].length + ' tests';
        directorySelector.appendChild(directoryOption);
    });

    selectDirectory();
}

/**
 * For a given failure type and directory and failure type, gets all the tests
 * in that directory and populatest the test selector with them.
 */
function selectDirectory()
{
    var selectedDirectory = getSelectValue('directory-selector');
    selectedTests = testsByDirectory[selectedDirectory];

    selectedTests.sort();

    var testSelector = $('test-selector');
    testSelector.innerHTML = '';

    selectedTests.forEach(function(testName) {
        var testOption = document.createElement('option');
        testOption.value = testName;
        var testDisplayName = testName;
        if (testName.lastIndexOf(selectedDirectory) == 0) {
            testDisplayName = testName.substring(selectedDirectory.length);
        }
        testOption.innerHTML = '&nbsp;&nbsp;' + testDisplayName;
        testSelector.appendChild(testOption);
    });

    selectTest();
}

function getSelectedTest()
{
    return getSelectValue('test-selector');
}

function selectTest()
{
    var selectedTest = getSelectedTest();

    if (results.tests[selectedTest].actual.indexOf('IMAGE') != -1) {
        $('image-outputs').style.display = '';
        displayImageResults(selectedTest);
    } else {
        $('image-outputs').style.display = 'none';
    }

    if (results.tests[selectedTest].actual.indexOf('TEXT') != -1) {
        $('text-outputs').style.display = '';
        displayTextResults(selectedTest);
    } else {
        $('text-outputs').style.display = 'none';
    }

    updateState();
    loupe.hide();
}

function updateState()
{
    var testName = getSelectedTest();
    var testIndex = selectedTests.indexOf(testName);
    var testCount = selectedTests.length
    $('test-index').textContent = testIndex + 1;
    $('test-count').textContent = testCount;

    $('next-test').disabled = testIndex == testCount - 1;
    $('previous-test').disabled = testIndex == 0;

    $('test-link').href =
        'http://trac.webkit.org/browser/trunk/LayoutTests/' + testName;
}

function getTestResultUrl(testName, mode)
{
    return '/test_result?test=' + testName + '&mode=' + mode;
}

var currentExpectedImageTest;
var currentActualImageTest;

function displayImageResults(testName)
{
    if (currentExpectedImageTest == currentActualImageTest
        && currentExpectedImageTest == testName) {
        return;
    }

    function displayImageResult(mode, callback) {
        var image = $(mode);
        image.className = 'loading';
        image.src = getTestResultUrl(testName, mode);
        image.onload = function() {
            image.className = '';
            callback();
            updateImageDiff();
        };
    }

    displayImageResult(
        'expected-image',
        function() { currentExpectedImageTest = testName; });
    displayImageResult(
        'actual-image',
        function() { currentActualImageTest = testName; });

    $('diff-canvas').className = 'loading';
    $('diff-canvas').style.display = '';
}

/**
 * Computes a graphical a diff between the expected and actual images by
 * rendering each to a canvas, getting the image data, and comparing the RGBA
 * components of each pixel. The output is put into the diff canvas, with
 * identical pixels appearing at 12.5% opacity and different pixels being
 * highlighted in red.
 */
function updateImageDiff() {
    if (currentExpectedImageTest != currentActualImageTest)
        return;

    var expectedImage = $('expected-image');
    var actualImage = $('actual-image');

    function getImageData(image) {
        var imageCanvas = document.createElement('canvas');
        imageCanvas.width = image.width;
        imageCanvas.height = image.height;
        imageCanvasContext = imageCanvas.getContext('2d');

        imageCanvasContext.fillStyle = 'rgba(255, 255, 255, 1)';
        imageCanvasContext.fillRect(
            0, 0, image.width, image.height);

        imageCanvasContext.drawImage(image, 0, 0);
        return imageCanvasContext.getImageData(
            0, 0, image.width, image.height);
    }

    var expectedImageData = getImageData(expectedImage);
    var actualImageData = getImageData(actualImage);

    var diffCanvas = $('diff-canvas');
    var diffCanvasContext = diffCanvas.getContext('2d');
    var diffImageData =
        diffCanvasContext.createImageData(diffCanvas.width, diffCanvas.height);

    // Avoiding property lookups for all these during the per-pixel loop below
    // provides a significant performance benefit.
    var expectedWidth = expectedImage.width;
    var expectedHeight = expectedImage.height;
    var expected = expectedImageData.data;

    var actualWidth = actualImage.width;
    var actual = actualImageData.data;

    var diffWidth = diffImageData.width;
    var diff = diffImageData.data;

    for (var x = 0; x < expectedWidth; x++) {
        for (var y = 0; y < expectedHeight; y++) {
            var expectedOffset = (y * expectedWidth + x) * 4;
            var actualOffset = (y * actualWidth + x) * 4;
            var diffOffset = (y * diffWidth + x) * 4;
            if (expected[expectedOffset] != actual[actualOffset] ||
                expected[expectedOffset + 1] != actual[actualOffset + 1] ||
                expected[expectedOffset + 2] != actual[actualOffset + 2] ||
                expected[expectedOffset + 3] != actual[actualOffset + 3]) {
                diff[diffOffset] = 255;
                diff[diffOffset + 1] = 0;
                diff[diffOffset + 2] = 0;
                diff[diffOffset + 3] = 255;
            } else {
                diff[diffOffset] = expected[expectedOffset];
                diff[diffOffset + 1] = expected[expectedOffset + 1];
                diff[diffOffset + 2] = expected[expectedOffset + 2];
                diff[diffOffset + 3] = 32;
            }
        }
    }

    diffCanvasContext.putImageData(
        diffImageData,
        0, 0,
        0, 0,
        diffImageData.width, diffImageData.height);
    diffCanvas.className = '';
}

function displayTextResults(testName)
{
    function loadTextResult(mode) {
        loadText(getTestResultUrl(testName, mode), function(text) {
            $(mode).textContent = text;
        });
    }

    loadTextResult('expected-text');
    loadTextResult('actual-text');
    loadTextResult('diff-text');
}

function nextTest()
{
    var testSelector = $('test-selector');
    var nextTestIndex = testSelector.selectedIndex + 1;
    while (true) {
        if (nextTestIndex == testSelector.options.length) {
            return;
        }
        if (testSelector.options[nextTestIndex].disabled) {
            nextTestIndex++;
        } else {
            testSelector.selectedIndex = nextTestIndex;
            selectTest();
            return;
        }
    }
}

function previousTest()
{
    var testSelector = $('test-selector');
    var previousTestIndex = testSelector.selectedIndex - 1;
    while (true) {
        if (previousTestIndex == -1) {
            return;
        }
        if (testSelector.options[previousTestIndex].disabled) {
            previousTestIndex--;
        } else {
            testSelector.selectedIndex = previousTestIndex;
            selectTest();
            return
        }
    }
}

window.addEventListener('DOMContentLoaded', main);
