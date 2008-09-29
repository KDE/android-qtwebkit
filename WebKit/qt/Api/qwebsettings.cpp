/*
    Copyright (C) 2008 Nokia Corporation and/or its subsidiary(-ies)

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#include "config.h"
#include "qwebsettings.h"

#include "qwebpage.h"
#include "qwebpage_p.h"

#include "Cache.h"
#include "Page.h"
#include "PageCache.h"
#include "Settings.h"
#include "KURL.h"
#include "PlatformString.h"
#include "IconDatabase.h"
#include "Image.h"
#include "IntSize.h"

#include <QHash>
#include <QSharedData>
#include <QUrl>
#include <QFileInfo>

class QWebSettingsPrivate
{
public:
    QWebSettingsPrivate(WebCore::Settings *wcSettings = 0)
        : settings(wcSettings)
    {
    }

    QHash<int, QString> fontFamilies;
    QHash<int, int> fontSizes;
    QHash<int, bool> attributes;
    QUrl userStyleSheetLocation;

    void apply();
    WebCore::Settings *settings;
};

typedef QHash<int, QPixmap> WebGraphicHash;
Q_GLOBAL_STATIC(WebGraphicHash, _graphics)

static WebGraphicHash* graphics()
{
    WebGraphicHash* hash = _graphics();

    if (hash->isEmpty()) {
        hash->insert(QWebSettings::MissingImageGraphic, QPixmap(QLatin1String(":webkit/resources/missingImage.png")));
        hash->insert(QWebSettings::MissingPluginGraphic, QPixmap(QLatin1String(":webkit/resources/nullPlugin.png")));
        hash->insert(QWebSettings::DefaultFrameIconGraphic, QPixmap(QLatin1String(":webkit/resources/urlIcon.png")));
        hash->insert(QWebSettings::TextAreaSizeGripCornerGraphic, QPixmap(QLatin1String(":webkit/resources/textAreaResizeCorner.png")));
    }

    return hash;
}

Q_GLOBAL_STATIC(QList<QWebSettingsPrivate *>, allSettings);

void QWebSettingsPrivate::apply()
{
    if (settings) {
        settings->setTextAreasAreResizable(true);

        QWebSettingsPrivate *global = QWebSettings::globalSettings()->d;

        QString family = fontFamilies.value(QWebSettings::StandardFont,
                                            global->fontFamilies.value(QWebSettings::StandardFont));
        settings->setStandardFontFamily(family);

        family = fontFamilies.value(QWebSettings::FixedFont,
                                    global->fontFamilies.value(QWebSettings::FixedFont));
        settings->setFixedFontFamily(family);

        family = fontFamilies.value(QWebSettings::SerifFont,
                                    global->fontFamilies.value(QWebSettings::SerifFont));
        settings->setSerifFontFamily(family);

        family = fontFamilies.value(QWebSettings::SansSerifFont,
                                    global->fontFamilies.value(QWebSettings::SansSerifFont));
        settings->setSansSerifFontFamily(family);

        family = fontFamilies.value(QWebSettings::CursiveFont,
                                    global->fontFamilies.value(QWebSettings::CursiveFont));
        settings->setCursiveFontFamily(family);

        family = fontFamilies.value(QWebSettings::FantasyFont,
                                    global->fontFamilies.value(QWebSettings::FantasyFont));
        settings->setFantasyFontFamily(family);

        int size = fontSizes.value(QWebSettings::MinimumFontSize,
                                   global->fontSizes.value(QWebSettings::MinimumFontSize));
        settings->setMinimumFontSize(size);

        size = fontSizes.value(QWebSettings::MinimumLogicalFontSize,
                                   global->fontSizes.value(QWebSettings::MinimumLogicalFontSize));
        settings->setMinimumLogicalFontSize(size);

        size = fontSizes.value(QWebSettings::DefaultFontSize,
                                   global->fontSizes.value(QWebSettings::DefaultFontSize));
        settings->setDefaultFontSize(size);

        size = fontSizes.value(QWebSettings::DefaultFixedFontSize,
                                   global->fontSizes.value(QWebSettings::DefaultFixedFontSize));
        settings->setDefaultFixedFontSize(size);

        bool value = attributes.value(QWebSettings::AutoLoadImages,
                                      global->attributes.value(QWebSettings::AutoLoadImages));
        settings->setLoadsImagesAutomatically(value);

        value = attributes.value(QWebSettings::JavascriptEnabled,
                                      global->attributes.value(QWebSettings::JavascriptEnabled));
        settings->setJavaScriptEnabled(value);

        value = attributes.value(QWebSettings::JavascriptCanOpenWindows,
                                      global->attributes.value(QWebSettings::JavascriptCanOpenWindows));
        settings->setJavaScriptCanOpenWindowsAutomatically(value);

        value = attributes.value(QWebSettings::JavaEnabled,
                                      global->attributes.value(QWebSettings::JavaEnabled));
        settings->setJavaEnabled(value);

        value = attributes.value(QWebSettings::PluginsEnabled,
                                      global->attributes.value(QWebSettings::PluginsEnabled));
        settings->setPluginsEnabled(value);

        value = attributes.value(QWebSettings::PrivateBrowsingEnabled,
                                      global->attributes.value(QWebSettings::PrivateBrowsingEnabled));
        settings->setPrivateBrowsingEnabled(value);

        value = attributes.value(QWebSettings::JavascriptCanAccessClipboard,
                                      global->attributes.value(QWebSettings::JavascriptCanAccessClipboard));
        settings->setDOMPasteAllowed(value);

        value = attributes.value(QWebSettings::DeveloperExtrasEnabled,
                                      global->attributes.value(QWebSettings::DeveloperExtrasEnabled));
        settings->setDeveloperExtrasEnabled(value);

        QUrl location = !userStyleSheetLocation.isEmpty() ? userStyleSheetLocation : global->userStyleSheetLocation;
        settings->setUserStyleSheetLocation(WebCore::KURL(location));

        value = attributes.value(QWebSettings::ZoomTextOnly,
                                 global->attributes.value(QWebSettings::ZoomTextOnly));
        settings->setZoomsTextOnly(value);
    } else {
        QList<QWebSettingsPrivate *> settings = *::allSettings();
        for (int i = 0; i < settings.count(); ++i)
            settings[i]->apply();
    }
}

/*!
    Returns the global settings object.

    Any setting changed on the default object is automatically applied to all
    QWebPage instances where the particular setting is not overriden already.
*/
QWebSettings *QWebSettings::globalSettings()
{
    static QWebSettings *global = 0;
    if (!global)
        global = new QWebSettings;
    return global;
}

/*!
    \class QWebSettings
    \since 4.4
    \brief The QWebSettings class provides an object to store the settings used
    by QWebPage and QWebFrame.

    Each QWebPage object has its own QWebSettings object, which configures the
    settings for that page. If a setting is not configured, then it is looked
    up in the global settings object, which can be accessed using
    QWebSettings::globalSettings().

    QWebSettings allows configuring font properties such as font size and font
    family, the location of a custom stylesheet, and generic attributes like java
    script, plugins, etc. The \l{QWebSettings::WebAttribute}{WebAttribute}
    enum further describes this.
    
    QWebSettings also configures global properties such as the web page memory
    cache and the web page icon database.

    \sa QWebPage::settings(), QWebView::settings(), {Browser}
*/

/*!
    \enum QWebSettings::FontFamily

    This enum describes the generic font families defined by CSS 2.
    For more information see the
    \l{http://www.w3.org/TR/REC-CSS2/fonts.html#generic-font-families}{CSS standard}.

    \value StandardFont
    \value FixedFont
    \value SerifFont
    \value SansSerifFont
    \value CursiveFont
    \value FantasyFont
*/

/*!
    \enum QWebSettings::FontSize

    This enum describes the font sizes configurable through QWebSettings.

    \value MinimumFontSize The hard minimum font size.
    \value MinimumLogicalFontSize The minimum logical font size that is applied
        after zooming with QWebFrame's textSizeMultiplier().
    \value DefaultFontSize The default font size for regular text.
    \value DefaultFixedFontSize The default font size for fixed-pitch text.
*/

/*!
    \enum QWebSettings::WebGraphic

    This enums describes the standard graphical elements used in webpages.

    \value MissingImageGraphic The replacement graphic shown when an image could not be loaded.
    \value MissingPluginGraphic The replacement graphic shown when a plugin could not be loaded.
    \value DefaultFrameIconGraphic The default icon for QWebFrame::icon().
    \value TextAreaSizeGripCornerGraphic The graphic shown for the size grip of text areas.
*/

/*!
    \enum QWebSettings::WebAttribute

    This enum describes various attributes that are configurable through QWebSettings.

    \value AutoLoadImages Specifies whether images are automatically loaded in
        web pages.
    \value JavascriptEnabled Enables or disables the running of JavaScript
        programs.
    \value JavaEnabled Enables or disables Java applets.
        Currently Java applets are not supported.
    \value PluginsEnabled Enables or disables plugins in web pages.
        Currently Flash and other plugins are not supported.
    \value PrivateBrowsingEnabled Private browsing prevents WebKit from
        recording visited pages in the history and storing web page icons.
    \value JavascriptCanOpenWindows Specifies whether JavaScript programs
        can open new windows.
    \value JavascriptCanAccessClipboard Specifies whether JavaScript programs
        can read or write to the clipboard.
    \value DeveloperExtrasEnabled Enables extra tools for Web developers.
        Currently this enables the "Inspect" element in the context menu,
    which shows the WebKit WebInspector for web site debugging.
    \value LinksIncludedInFocusChain Specifies whether hyperlinks should be
        included in the keyboard focus chain.
    \value ZoomTextOnly Specifies whether the zoom factor on a frame applies to
        only the text or all content.
*/

/*!
    \internal
*/
QWebSettings::QWebSettings()
    : d(new QWebSettingsPrivate)
{
    // Initialize our global defaults
    // changing any of those will likely break the LayoutTests
    d->fontSizes.insert(QWebSettings::MinimumFontSize, 5);
    d->fontSizes.insert(QWebSettings::MinimumLogicalFontSize, 5);
    d->fontSizes.insert(QWebSettings::DefaultFontSize, 14);
    d->fontSizes.insert(QWebSettings::DefaultFixedFontSize, 14);
    d->fontFamilies.insert(QWebSettings::StandardFont, QLatin1String("Arial"));
    d->fontFamilies.insert(QWebSettings::FixedFont, QLatin1String("Courier New"));
    d->fontFamilies.insert(QWebSettings::SerifFont, QLatin1String("Times New Roman"));
    d->fontFamilies.insert(QWebSettings::SansSerifFont, QLatin1String("Arial"));
    d->fontFamilies.insert(QWebSettings::CursiveFont, QLatin1String("Arial"));
    d->fontFamilies.insert(QWebSettings::FantasyFont, QLatin1String("Arial"));

    d->attributes.insert(QWebSettings::AutoLoadImages, true);
    d->attributes.insert(QWebSettings::JavascriptEnabled, true);
    d->attributes.insert(QWebSettings::LinksIncludedInFocusChain, true);
    d->attributes.insert(QWebSettings::ZoomTextOnly, false);
}

/*!
    \internal
*/
QWebSettings::QWebSettings(WebCore::Settings *settings)
    : d(new QWebSettingsPrivate(settings))
{
    d->settings = settings;
    d->apply();
    allSettings()->append(d);
}

/*!
    \internal
*/
QWebSettings::~QWebSettings()
{
    if (d->settings)
        allSettings()->removeAll(d);

    delete d;
}

/*!
    Sets the font size for \a type to \a size.
*/
void QWebSettings::setFontSize(FontSize type, int size)
{
    d->fontSizes.insert(type, size);
    d->apply();
}

/*!
    Returns the default font size for \a type.
*/
int QWebSettings::fontSize(FontSize type) const
{
    int defaultValue = 0;
    if (d->settings) {
        QWebSettingsPrivate *global = QWebSettings::globalSettings()->d;
        defaultValue = global->fontSizes.value(type);
    }
    return d->fontSizes.value(type, defaultValue);
}

/*!
    Resets the font size for \a type to the size specified in the global
    settings object.

    This function has no effect on the global QWebSettings instance.
*/
void QWebSettings::resetFontSize(FontSize type)
{
    if (d->settings) {
        d->fontSizes.remove(type);
        d->apply();
    }
}

/*!
    Specifies the location of a user stylesheet to load with every web page.

    The \a location can be a URL or a path on the local filesystem.

    \sa userStyleSheetUrl()
*/
void QWebSettings::setUserStyleSheetUrl(const QUrl &location)
{
    d->userStyleSheetLocation = location;
    d->apply();
}

/*!
    Returns the location of the user stylesheet.

    \sa setUserStyleSheetUrl()
*/
QUrl QWebSettings::userStyleSheetUrl() const
{
    return d->userStyleSheetLocation;
}

/*!
    Sets the path of the icon database to \a path. The icon database is used
    to store "favicons" associated with web sites.

    \a path must point to an existing directory where the icons are stored.

    Setting an empty path disables the icon database.
*/
void QWebSettings::setIconDatabasePath(const QString &path)
{
    WebCore::iconDatabase()->delayDatabaseCleanup();

    if (!path.isEmpty()) {
        WebCore::iconDatabase()->setEnabled(true);
        QFileInfo info(path);
        if (info.isDir() && info.isWritable())
            WebCore::iconDatabase()->open(path);
    } else {
        WebCore::iconDatabase()->setEnabled(false);
        WebCore::iconDatabase()->close();
    }
}

/*!
    Returns the path of the icon database or an empty string if the icon
    database is disabled.

    \sa setIconDatabasePath(), clearIconDatabase()
*/
QString QWebSettings::iconDatabasePath()
{
    if (WebCore::iconDatabase()->isEnabled() && WebCore::iconDatabase()->isOpen()) {
        return WebCore::iconDatabase()->databasePath();
    } else {
        return QString();
    }
}

/*!
    Clears the icon database.
*/
void QWebSettings::clearIconDatabase()
{
    if (WebCore::iconDatabase()->isEnabled() && WebCore::iconDatabase()->isOpen())
        WebCore::iconDatabase()->removeAllIcons();
}

/*!
    Returns the web site's icon for \a url.

    If the web site does not specify an icon, or the icon is not in the
    database, a null QIcon is returned.

    \note The returned icon's size is arbitrary.

    \sa setIconDatabasePath()
*/
QIcon QWebSettings::iconForUrl(const QUrl &url)
{
    WebCore::Image* image = WebCore::iconDatabase()->iconForPageURL(WebCore::KURL(url).string(),
                                WebCore::IntSize(16, 16));
    if (!image) {
        return QPixmap();
    }
    QPixmap *icon = image->nativeImageForCurrentFrame();
    if (!icon) {
        return QPixmap();
    }
    return *icon;
}

/*!
    Sets \a graphic to be drawn when QtWebKit needs to draw an image of the
    given \a type.

    For example, when an image cannot be loaded the pixmap specified by
    \l{QWebSettings::WebGraphic}{MissingImageGraphic} is drawn instead.

    \sa webGraphic()
*/
void QWebSettings::setWebGraphic(WebGraphic type, const QPixmap &graphic)
{
    WebGraphicHash *h = graphics();
    if (graphic.isNull())
        h->remove(type);
    else
        h->insert(type, graphic);
}

/*!
    Returns a previously set pixmap used to draw replacement graphics of the
    specified \a type.

    For example, when an image cannot be loaded the pixmap specified by
    \l{QWebSettings::WebGraphic}{MissingImageGraphic} is drawn instead.

    \sa setWebGraphic()
*/
QPixmap QWebSettings::webGraphic(WebGraphic type)
{
    return graphics()->value(type);
}

/*!
    Sets the maximum number of pages to hold in the memory cache to \a pages.
*/
void QWebSettings::setMaximumPagesInCache(int pages)
{
    WebCore::pageCache()->setCapacity(qMax(0, pages));
}

/*!
    Returns the maximum number of web pages that are kept in the memory cache.
*/
int QWebSettings::maximumPagesInCache()
{
    return WebCore::pageCache()->capacity();
}

/*!
   Specifies the capacities for the memory cache for dead objects such as
   stylesheets or scripts.

   The \a cacheMinDeadCapacity specifies the \e minimum number of bytes that
   dead objects should consume when the cache is under pressure.
   
   \a cacheMaxDead is the \e maximum number of bytes that dead objects should
   consume when the cache is \bold not under pressure.

   \a totalCapacity specifies the \e maximum number of bytes that the cache
   should consume \bold overall.

   The cache is enabled by default. Calling setObjectCacheCapacities(0, 0, 0)
   will disable the cache. Calling it with one non-zero enables it again.
*/
void QWebSettings::setObjectCacheCapacities(int cacheMinDeadCapacity, int cacheMaxDead, int totalCapacity)
{
    bool disableCache = cacheMinDeadCapacity == 0 && cacheMaxDead == 0 && totalCapacity == 0;
    WebCore::cache()->setDisabled(disableCache);

    WebCore::cache()->setCapacities(qMax(0, cacheMinDeadCapacity),
                                    qMax(0, cacheMaxDead),
                                    qMax(0, totalCapacity));
}

/*!
    Sets the actual font family to \a family for the specified generic family,
    \a which.
*/
void QWebSettings::setFontFamily(FontFamily which, const QString &family)
{
    d->fontFamilies.insert(which, family);
    d->apply();
}

/*!
    Returns the actual font family for the specified generic font family,
    \a which.
*/
QString QWebSettings::fontFamily(FontFamily which) const
{
    QString defaultValue;
    if (d->settings) {
        QWebSettingsPrivate *global = QWebSettings::globalSettings()->d;
        defaultValue = global->fontFamilies.value(which);
    }
    return d->fontFamilies.value(which, defaultValue);
}

/*!
    Resets the actual font family to the default font family, specified by
    \a which.

    This function has no effect on the global QWebSettings instance.
*/
void QWebSettings::resetFontFamily(FontFamily which)
{
    if (d->settings) {
        d->fontFamilies.remove(which);
        d->apply();
    }
}

/*!
    \fn void QWebSettings::setAttribute(WebAttribute attribute, bool on)
    
    Enables or disables the specified \a attribute feature depending on the
    value of \a on.
*/
void QWebSettings::setAttribute(WebAttribute attr, bool on)
{
    d->attributes.insert(attr, on);
    d->apply();
}

/*!
    \fn bool QWebSettings::testAttribute(WebAttribute attribute) const

    Returns true if \a attribute is enabled; otherwise returns false.
*/
bool QWebSettings::testAttribute(WebAttribute attr) const
{
    bool defaultValue = false;
    if (d->settings) {
        QWebSettingsPrivate *global = QWebSettings::globalSettings()->d;
        defaultValue = global->attributes.value(attr);
    }
    return d->attributes.value(attr, defaultValue);
}

/*!
    \fn void QWebSettings::resetAttribute(WebAttribute attribute)

    Resets the setting of \a attribute.
    This function has no effect on the global QWebSettings instance.

    \sa globalSettings()
*/
void QWebSettings::resetAttribute(WebAttribute attr)
{
    if (d->settings) {
        d->attributes.remove(attr);
        d->apply();
    }
}

