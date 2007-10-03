# Include file to make it easy to include WebKit into Qt projects


isEmpty(OUTPUT_DIR) {
    CONFIG(release):OUTPUT_DIR=$$PWD/WebKitBuild/Release
    CONFIG(debug):OUTPUT_DIR=$$PWD/WebKitBuild/Debug
}

!gtk-port:CONFIG += qt-port
qt-port:DEFINES += BUILDING_QT__=1
qt-port:!building-libs {
    QMAKE_LIBDIR = $$OUTPUT_DIR/lib $$QMAKE_LIBDIR
    LIBS += -lQtWebKit
}
gtk-port:CONFIG += link_pkgconfig
gtk-port:PKGCONFIG += cairo cairo-ft gdk-2.0 gtk+-2.0 libcurl
gtk-port:DEFINES += BUILDING_GTK__=1 BUILDING_CAIRO__
gtk-port:LIBS += -L$$OUTPUT_DIR/lib -lWebKitGtk $$system(icu-config --ldflags) -ljpeg -lpng
gtk-port:QMAKE_CXXFLAGS += $$system(icu-config --cppflags)

DEFINES += USE_SYSTEM_MALLOC
CONFIG(release) {
    DEFINES += NDEBUG
}

BASE_DIR = $$PWD
qt-port:INCLUDEPATH += \
    $$PWD/WebKit/qt/Api
gtk-port:INCLUDEPATH += \
    $$BASE_DIR/WebCore/platform/gtk \
    $$BASE_DIR/WebCore/platform/network/curl \
    $$BASE_DIR/WebCore/platform/graphics/cairo \
    $$BASE_DIR/WebCore/loader/gtk \
    $$BASE_DIR/WebCore/page/gtk \
    $$BASE_DIR/WebKit/gtk/Api \
    $$BASE_DIR/WebKit/gtk/WebCoreSupport
INCLUDEPATH += \
    $$BASE_DIR/JavaScriptCore/ \
    $$BASE_DIR/JavaScriptCore/kjs \
    $$BASE_DIR/JavaScriptCore/bindings \
    $$BASE_DIR/JavaScriptCore/bindings/c \
    $$BASE_DIR/JavaScriptCore/wtf \
    $$BASE_DIR/JavaScriptCore/ForwardingHeaders \
    $$BASE_DIR/WebCore \
    $$BASE_DIR/WebCore/ForwardingHeaders \
    $$BASE_DIR/WebCore/platform \
    $$BASE_DIR/WebCore/platform/network \
    $$BASE_DIR/WebCore/platform/graphics \
    $$BASE_DIR/WebCore/loader \
    $$BASE_DIR/WebCore/page \
    $$BASE_DIR/WebCore/css \
    $$BASE_DIR/WebCore/dom \
    $$BASE_DIR/WebCore/bridge \
    $$BASE_DIR/WebCore/editing \
    $$BASE_DIR/WebCore/rendering \
    $$BASE_DIR/WebCore/history \
    $$BASE_DIR/WebCore/xml \
    $$BASE_DIR/WebCore/html


macx {
	INCLUDEPATH += /usr/include/libxml2
	LIBS += -lxml2 -lxslt
}
