description("Test IndexedDB's openCursor.");
if (window.layoutTestController)
    layoutTestController.waitUntilDone();

function emptyCursorSuccess()
{
    debug("Empty cursor opened successfully.")
    verifySuccessEvent(event);
    // FIXME: check that we can iterate the cursor.
    shouldBe("event.result", "null");
    done();
}

function openEmptyCursor()
{
    debug("Opening an empty cursor.");
    keyRange = IDBKeyRange.leftBound("InexistentKey");
    result = evalAndLog("objectStore.openCursor(keyRange)");
    verifyResult(result);
    result.onsuccess = emptyCursorSuccess;
}

function cursorSuccess()
{
    debug("Cursor opened successfully.")
    verifySuccessEvent(event);
    // FIXME: check that we can iterate the cursor.
    shouldBe("event.result.direction", "0");
    shouldBe("event.result.key", "'myKey'");
    shouldBe("event.result.value", "'myValue'");
    debug("");
    openEmptyCursor();
}

function openCursor()
{
    debug("Opening cursor");
    keyRange = IDBKeyRange.leftBound("myKey");
    result = evalAndLog("objectStore.openCursor(keyRange)");
    verifyResult(result);
    result.onsuccess = cursorSuccess;
}

function populateObjectStore(objectStore)
{
    result = evalAndLog("objectStore.add('myValue', 'myKey')");
    verifyResult(result);
    result.onsuccess = openCursor;
    result.onerror = unexpectedErrorCallback;
}

function createObjectStoreSuccess()
{
    verifySuccessEvent(event);
    var objectStore = evalAndLog("objectStore = event.result");
    populateObjectStore(objectStore);
}

function openSuccess()
{
    verifySuccessEvent(event);
    var db = evalAndLog("db = event.result");

    deleteAllObjectStores(db);

    result = evalAndLog("db.createObjectStore('test')");
    verifyResult(result);
    result.onsuccess = createObjectStoreSuccess;
    result.onerror = unexpectedErrorCallback;
}

function test()
{
    result = evalAndLog("indexedDB.open('name', 'description')");
    verifyResult(result);
    result.onsuccess = openSuccess;
    result.onerror = unexpectedErrorCallback;
}

test();

var successfullyParsed = true;
