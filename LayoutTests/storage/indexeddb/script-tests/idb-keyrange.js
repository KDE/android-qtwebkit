description("Test IndexedDB's KeyRange.");
if (window.layoutTestController)
    layoutTestController.waitUntilDone();

function checkSingleKeyRange(value)
{
    keyRange = evalAndLog("indexedDB.makeSingleKeyRange(" + value + ")");
    shouldBe("keyRange.left", "" + value);
    shouldBe("keyRange.right", "" + value);
    shouldBe("keyRange.flags", "keyRange.SINGLE");
}

function checkLeftBoundKeyRange(value, open)
{
    keyRange = evalAndLog("indexedDB.makeLeftBoundKeyRange(" + value + "," + open + ")");
    shouldBe("keyRange.left", "" + value);
    shouldBeNull("keyRange.right");
    shouldBe("keyRange.flags", open ? "keyRange.LEFT_OPEN" : "keyRange.LEFT_BOUND");
}

function checkRightBoundKeyRange(value, open)
{
    keyRange = evalAndLog("indexedDB.makeRightBoundKeyRange(" + value + "," + open + ")");
    shouldBe("keyRange.right", "" + value);
    shouldBeNull("keyRange.left");
    shouldBe("keyRange.flags", open ? "keyRange.RIGHT_OPEN" : "keyRange.RIGHT_BOUND");
}

function checkBoundKeyRange(left, right, openLeft, openRight)
{
    keyRange = evalAndLog("indexedDB.makeBoundKeyRange(" + left + "," + right + "," + openLeft + "," + openRight + ")");
    shouldBe("keyRange.left", "" + left);
    shouldBe("keyRange.right", "" + right);
    leftFlags = keyRange.flags & (keyRange.LEFT_OPEN | keyRange.LEFT_BOUND);
    shouldBe("leftFlags", openLeft ? "keyRange.LEFT_OPEN" : "keyRange.LEFT_BOUND");
    rightFlags = keyRange.flags & (keyRange.RIGHT_OPEN | keyRange.RIGHT_BOUND);
    shouldBe("rightFlags", openRight ? "keyRange.RIGHT_OPEN" : "keyRange.RIGHT_BOUND");
}

function test()
{
    checkSingleKeyRange(1);
    checkSingleKeyRange("'a'");

    checkLeftBoundKeyRange(10, true);
    checkLeftBoundKeyRange(11, false);
    checkLeftBoundKeyRange(12);
    checkLeftBoundKeyRange("'aa'", true);
    checkLeftBoundKeyRange("'ab'", false);
    checkLeftBoundKeyRange("'ac'");

    checkRightBoundKeyRange(20, true);
    checkRightBoundKeyRange(21, false);
    checkRightBoundKeyRange(22);
    checkRightBoundKeyRange("'ba'", true);
    checkRightBoundKeyRange("'bb'", false);
    checkRightBoundKeyRange("'bc'");

    checkBoundKeyRange(30, 40);
    checkBoundKeyRange(31, 41, false, false);
    checkBoundKeyRange(32, 42, false, true);
    checkBoundKeyRange(33, 43, true, false);
    checkBoundKeyRange(34, 44, true, true);

    checkBoundKeyRange("'aaa'", "'aba'", false, false);
    checkBoundKeyRange("'aab'", "'abb'");
    checkBoundKeyRange("'aac'", "'abc'", false, false);
    checkBoundKeyRange("'aad'", "'abd'", false, true);
    checkBoundKeyRange("'aae'", "'abe'", true, false);
    checkBoundKeyRange("'aaf'", "'abf'", true, true);

    done();
}

test();
