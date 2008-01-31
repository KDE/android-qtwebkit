function log(s)
{
    document.getElementById("console").appendChild(document.createTextNode(s + "\n"));
}

function shouldBe(a, b, shouldNotPrintValues)
{
    var evalA, evalB;
    try {
        evalA = eval(a);
        evalB = eval(b);
    } catch(e) {
        evalA = e;
    }

    var message;
    if (evalA === evalB) {
        message = "PASS";
        if (!shouldNotPrintValues) {
            message += ": " + a + " should be '" + evalB + "' and is.";
        } else {
            message += ": " + a + " matched the expected value.";
        }
    } else {
       message = "*** FAIL: " + a + " should be '" + evalB + "' but instead is " + evalA + ". ***";
    }

    message = String(message).replace(/\n/g, "");
    log(message);
}

function shouldBeTrue(a) 
{ 
    shouldBe(a, "true"); 
}

function shouldBeFalse(b) 
{ 
    shouldBe(b, "false"); 
}

function canGet(keyPath)
{
    try {
        return eval("window." + keyPath) !== undefined;
    } catch(e) {
        return false;
    }
}

window.marker = { };

function canSet(keyPath, valuePath)
{
    if (valuePath === undefined)
        valuePath = "window.marker";

    try {
        eval("window." + keyPath + " = " + valuePath);
        return eval("window." + keyPath) === eval("window." + valuePath);
    } catch(e) {
        return false;
    }
}

function canCall(keyPath, argumentString)
{
    try {
        eval("window." + keyPath + "(" + (argumentString === undefined ? "" : "'" + argumentString + "'") + ")");
        return true;
    } catch(e) {
        return false;
    }
}

function toString(expression, valueForException)
{
    if (valueForException === undefined)
        valueForException = "[exception]";
        
    try {
        var evalExpression = eval(expression);
        if (evalExpression === undefined)
            throw null;
        return String(evalExpression);
    } catch(e) {
        return valueForException;
    }
}

// Frame Access Tests

function canAccessFrame(iframeURL, iframeId, passMessage, failMessage) {
    if (window.layoutTestController) {
        layoutTestController.dumpAsText();
        layoutTestController.dumpChildFramesAsText();
        layoutTestController.waitUntilDone();
    }

    var targetWindow = frames[0];
    if (!targetWindow.document.body)
        log("FAIL: targetWindow started with no document, we won't know if the test passed or failed.");

    var iframe = document.getElementById(iframeId);
    iframe.src = iframeURL;

    var testDone = false;

    setTimeout(test, 1);

    setTimeout(function() {
        if (!testDone) {
            log(failMessage);
            if (window.layoutTestController)
                layoutTestController.notifyDone();
        }
    }, 2000);

    function test() {
        try {
            if (targetWindow.document.body) {
                if (targetWindow.document.getElementById('accessMe')) {
                    targetWindow.document.getElementById('accessMe').innerHTML = passMessage;
                    log(passMessage);
                    testDone = true;
                    if (window.layoutTestController)
                        layoutTestController.notifyDone();
                    return;
                }
            }
        } catch (e) {
            log("In catch");
        }

        setTimeout(test, 1);
    }
}

function cannotAccessFrame(iframeURL, iframeId, passMessage, failMessage) {
    if (window.layoutTestController) {
        layoutTestController.dumpAsText();
        layoutTestController.dumpChildFramesAsText();
        layoutTestController.waitUntilDone();
    }

    var targetWindow = frames[0];
    if (!targetWindow.document.body)
        log("FAIL: targetWindow started with no document, we won't know if the test passed or failed.");

    var iframe = document.getElementById(iframeId);
    iframe.src = iframeURL;

    var testDone = false;

    setTimeout(test, 1);

    setTimeout(function() {
        if (!testDone) {
            log(failMessage);
            window.stop();
            if (window.layoutTestController)
                layoutTestController.notifyDone();
        }
    }, 2000);

    function test() {
        try {
            if (targetWindow.document.body) {
                if (targetWindow.document.getElementById('accessMe')) {
                    targetWindow.document.getElementById('accessMe').innerHTML = failMessage;
                    log(failMessage);
                    testDone = true;
                    window.stop();
                    if (window.layoutTestController)
                        layoutTestController.notifyDone();
                    return;
                }

                setTimeout(test, 1);
                return;
            }
        } catch (e) {
        }

        log(passMessage);
        testDone = true;
        window.stop();
        if (window.layoutTestController)
            layoutTestController.notifyDone();
    }
}

function closeWindowAndNotifyDone(win)
{
    win.close();
    setTimeout(doneHandler, 1);
    function doneHandler() {
        if (win.closed) {
            if (window.layoutTestController)
                layoutTestController.notifyDone();
            return;
        }

        setTimeout(doneHandler, 1);
    }
}
