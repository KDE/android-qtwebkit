function done()
{
    isSuccessfullyParsed();
    if (window.layoutTestController)
        layoutTestController.notifyDone()
}

function verifyEventCommon(event)
{
    shouldBeTrue("'source' in event");
    shouldBeTrue("event.source != null");
    shouldBeTrue("'onsuccess' in event.target");
    shouldBeTrue("'onerror' in event.target");
    shouldBeTrue("'readyState' in event.target");
    shouldBe("event.target.readyState", "event.target.DONE");
    debug("");
}

function verifyErrorEvent(event)
{
    debug("Error event fired:");
    shouldBeFalse("'result' in event");
    shouldBeTrue("'code' in event");
    shouldBeTrue("'message' in event");
    verifyEventCommon(event);
}

function verifySuccessEvent(event)
{
    debug("Success event fired:");
    shouldBeTrue("'result' in event");
    shouldBeFalse("'code' in event");
    shouldBeFalse("'message' in event");
    verifyEventCommon(event);
}

function verifyAbortEvent(event)
{
    debug("Abort event fired:");
    shouldBeEqualToString("event.type", "abort");
}

function verifyResult(result)
{
    shouldBeTrue("'onsuccess' in result");
    shouldBeTrue("'onerror' in result");
    shouldBeTrue("'readyState' in result");
    debug("An event should fire shortly...");
    debug("");
}

function unexpectedSuccessCallback()
{
    testFailed("Success function called unexpectedly.");
    debug("");
    verifySuccessEvent(event);
    done();
}

function unexpectedErrorCallback()
{
    testFailed("Error function called unexpectedly: (" + event.code + ") " + event.message);
    debug("");
    verifyErrorEvent(event);
    done();
}

function deleteAllObjectStores(db)
{
    objectStores = db.objectStores;
    for (var i = 0; i < objectStores.length; ++i)
        db.removeObjectStore(objectStores[i]);
}
