TEMPLATE = subdirs
CONFIG += ordered

SUBDIRS += \
        JavaScriptCore \
        WebCore \
        JavaScriptCore/kjs/jsc.pro \
        WebKit/qt/QtLauncher \
        WebKit/qt/tests

!win32-*: SUBDIRS += WebKitTools/DumpRenderTree/qt/DumpRenderTree.pro

