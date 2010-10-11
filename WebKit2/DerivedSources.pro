TEMPLATE = lib
TARGET = dummy

CONFIG -= debug_and_release

WEBCORE_GENERATED_HEADERS_FOR_WEBKIT2 += \
    $$OUTPUT_DIR/WebCore/generated/HTMLNames.h \
    $$OUTPUT_DIR/WebCore/generated/JSCSSStyleDeclaration.h \
    $$OUTPUT_DIR/WebCore/generated/JSDOMWindow.h \
    $$OUTPUT_DIR/WebCore/generated/JSElement.h \
    $$OUTPUT_DIR/WebCore/generated/JSHTMLElement.h \
    $$OUTPUT_DIR/WebCore/generated/JSRange.h \


QUOTE = ""
DOUBLE_ESCAPED_QUOTE = ""
ESCAPE = ""
win32-msvc*|symbian {
    ESCAPE = "^"
} else:win32-g++*:isEmpty(QMAKE_SH) {
    # MinGW's make will run makefile commands using sh, even if make
    #  was run from the Windows shell, if it finds sh in the path.
    ESCAPE = "^"
} else {
    QUOTE = "\'"
    DOUBLE_ESCAPED_QUOTE = "\\\'"
}

DIRS = \
    $$OUTPUT_DIR/include/JavaScriptCore \
    $$OUTPUT_DIR/include/WebCore \
    $$OUTPUT_DIR/include/WebKit2 \
    $$OUTPUT_DIR/WebKit2/generated

for(DIR, DIRS) {
    !exists($$DIR): system($$QMAKE_MKDIR $$DIR)
}

QMAKE_EXTRA_TARGETS += createdirs

SRC_ROOT_DIR = $$replace(PWD, /WebKit2, /)

messageheader_generator.commands = python $${SRC_ROOT_DIR}/WebKit2/Scripts/generate-messages-header.py  $${SRC_ROOT_DIR}/WebKit2/WebProcess/WebPage/WebPage.messages.in > $$OUTPUT_DIR/WebKit2/generated/WebPageMessages.h
messageheader_generator.depends  = $${SRC_ROOT_DIR}/WebKit2/Scripts/generate-messages-header.py $${SRC_ROOT_DIR}/WebKit2/Scripts/webkit2/messages.py $${SRC_ROOT_DIR}/WebKit2/WebProcess/WebPage/WebPage.messages.in
messageheader_generator.target   = $${OUTPUT_DIR}/WebKit2/generated/WebPageMessages.h
generated_files.depends          += messageheader_generator
QMAKE_EXTRA_TARGETS              += messageheader_generator

messagereceiver_generator.commands = python $${SRC_ROOT_DIR}/WebKit2/Scripts/generate-message-receiver.py  $${SRC_ROOT_DIR}/WebKit2/WebProcess/WebPage/WebPage.messages.in > $$OUTPUT_DIR/WebKit2/generated/WebPageMessageReceiver.cpp
messagereceiver_generator.depends  = $${SRC_ROOT_DIR}/WebKit2/Scripts/generate-message-receiver.py $${SRC_ROOT_DIR}/WebKit2/Scripts/webkit2/messages.py $${SRC_ROOT_DIR}/WebKit2/WebProcess/WebPage/WebPage.messages.in
messagereceiver_generator.target   = $${OUTPUT_DIR}/WebKit2/generated/WebPageMessageReceiver.cpp
generated_files.depends            += messagereceiver_generator
QMAKE_EXTRA_TARGETS                += messagereceiver_generator

pageproxymessageheader_generator.commands = python $${SRC_ROOT_DIR}/WebKit2/Scripts/generate-messages-header.py  $${SRC_ROOT_DIR}/WebKit2/UIProcess/WebPageProxy.messages.in > $$OUTPUT_DIR/WebKit2/generated/WebPageProxyMessages.h
pageproxymessageheader_generator.depends  = $${SRC_ROOT_DIR}/WebKit2/Scripts/generate-messages-header.py $${SRC_ROOT_DIR}/WebKit2/Scripts/webkit2/messages.py $${SRC_ROOT_DIR}/WebKit2/UIProcess/WebPageProxy.messages.in
pageproxymessageheader_generator.target   = $${OUTPUT_DIR}/WebKit2/generated/WebPageProxyMessages.h
generated_files.depends                 += pageproxymessageheader_generator
QMAKE_EXTRA_TARGETS                     += pageproxymessageheader_generator

pageproxymessagereceiver_generator.commands = python $${SRC_ROOT_DIR}/WebKit2/Scripts/generate-message-receiver.py  $${SRC_ROOT_DIR}/WebKit2/UIProcess/WebPageProxy.messages.in > $$OUTPUT_DIR/WebKit2/generated/WebPageProxyMessageReceiver.cpp
pageproxymessagereceiver_generator.depends  = $${SRC_ROOT_DIR}/WebKit2/Scripts/generate-message-receiver.py $${SRC_ROOT_DIR}/WebKit2/Scripts/webkit2/messages.py $${SRC_ROOT_DIR}/WebKit2/UIProcess/WebPageProxy.messages.in
pageproxymessagereceiver_generator.target   = $${OUTPUT_DIR}/WebKit2/generated/WebPageProxyMessageReceiver.cpp
generated_files.depends                   += pageproxymessagereceiver_generator
QMAKE_EXTRA_TARGETS                       += pageproxymessagereceiver_generator

processmessageheader_generator.commands = python $${SRC_ROOT_DIR}/WebKit2/Scripts/generate-messages-header.py  $${SRC_ROOT_DIR}/WebKit2/WebProcess/WebProcess.messages.in > $$OUTPUT_DIR/WebKit2/generated/WebProcessMessages.h
processmessageheader_generator.depends  = $${SRC_ROOT_DIR}/WebKit2/Scripts/generate-messages-header.py $${SRC_ROOT_DIR}/WebKit2/Scripts/webkit2/messages.py $${SRC_ROOT_DIR}/WebKit2/WebProcess/WebProcess.messages.in
processmessageheader_generator.target   = $${OUTPUT_DIR}/WebKit2/generated/WebProcessMessages.h
generated_files.depends                 += processmessageheader_generator
QMAKE_EXTRA_TARGETS                     += processmessageheader_generator

processmessagereceiver_generator.commands = python $${SRC_ROOT_DIR}/WebKit2/Scripts/generate-message-receiver.py  $${SRC_ROOT_DIR}/WebKit2/WebProcess/WebProcess.messages.in > $$OUTPUT_DIR/WebKit2/generated/WebProcessMessageReceiver.cpp
processmessagereceiver_generator.depends  = $${SRC_ROOT_DIR}/WebKit2/Scripts/generate-message-receiver.py $${SRC_ROOT_DIR}/WebKit2/Scripts/webkit2/messages.py $${SRC_ROOT_DIR}/WebKit2/WebProcess/WebProcess.messages.in
processmessagereceiver_generator.target   = $${OUTPUT_DIR}/WebKit2/generated/WebProcessMessageReceiver.cpp
generated_files.depends                   += processmessagereceiver_generator
QMAKE_EXTRA_TARGETS                       += processmessagereceiver_generator

fwheader_generator.commands = perl $${SRC_ROOT_DIR}/WebKitTools/Scripts/generate-forwarding-headers.pl $${SRC_ROOT_DIR}/WebKit2 $${OUTPUT_DIR}/include qt
fwheader_generator.depends  = $${SRC_ROOT_DIR}/WebKitTools/Scripts/generate-forwarding-headers.pl
generated_files.depends     += fwheader_generator
QMAKE_EXTRA_TARGETS         += fwheader_generator

for(HEADER, WEBCORE_GENERATED_HEADERS_FOR_WEBKIT2) {
    HEADER_NAME = $$basename(HEADER)
    HEADER_PATH = $$HEADER
    HEADER_TARGET = $$replace(HEADER_PATH, [^a-zA-Z0-9_], -)
    HEADER_TARGET = "qtheader-$${HEADER_TARGET}"
    DESTDIR = $$OUTPUT_DIR/include/"WebCore"

    eval($${HEADER_TARGET}.target = $$DESTDIR/$$HEADER_NAME)
    eval($${HEADER_TARGET}.depends = $$HEADER_PATH)
    eval($${HEADER_TARGET}.commands = echo $${DOUBLE_ESCAPED_QUOTE}\$${LITERAL_HASH}include \\\"$$HEADER_PATH\\\"$${DOUBLE_ESCAPED_QUOTE} > $$eval($${HEADER_TARGET}.target))

    QMAKE_EXTRA_TARGETS += $$HEADER_TARGET
    generated_files.depends += $$eval($${HEADER_TARGET}.target)
}

QMAKE_EXTRA_TARGETS += generated_files
