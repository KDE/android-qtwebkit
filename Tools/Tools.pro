
TEMPLATE = subdirs
CONFIG += ordered

include($$PWD/../Source/common.pri)
include($$PWD/../Source/WebCore/features.pri)

exists($$PWD/QtTestBrowser/QtTestBrowser.pro): SUBDIRS += QtTestBrowser/QtTestBrowser.pro
!android: exists($$PWD/DumpRenderTree/qt/DumpRenderTree.pro): SUBDIRS += DumpRenderTree/qt/DumpRenderTree.pro
!android: exists($$PWD/DumpRenderTree/qt/ImageDiff.pro): SUBDIRS += DumpRenderTree/qt/ImageDiff.pro

webkit2 {
    exists($$PWD/MiniBrowser/qt/MiniBrowser.pro): SUBDIRS += MiniBrowser/qt/MiniBrowser.pro
    !android:!symbian:exists($$PWD/WebKitTestRunner/WebKitTestRunner.pro): SUBDIRS += WebKitTestRunner/WebKitTestRunner.pro
}

!android:!win32:!symbian:contains(DEFINES, ENABLE_NETSCAPE_PLUGIN_API=1) {
    exists($$PWD/DumpRenderTree/qt/TestNetscapePlugin/TestNetscapePlugin.pro): SUBDIRS += DumpRenderTree/qt/TestNetscapePlugin/TestNetscapePlugin.pro
}
