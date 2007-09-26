/*
    Copyright (C) 2007 Trolltech ASA

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

    This class provides all functionality needed for loading images, style sheets and html
    pages from the web. It has a memory cache for these objects.
*/

#include "qwebsettings.h"

#include "qwebpage.h"
#include "qwebpage_p.h"

#include "Page.h"
#include "Settings.h"
#include "KURL.h"
#include "PlatformString.h"
#include "IconDatabase.h"

#include <QHash>
#include <QSharedData>

class QWebSettingsPrivate : public QSharedData
{
public:
    QWebSettingsPrivate()
        : minimumFontSize(5),
          minimumLogicalFontSize(5),
          defaultFontSize(14),
          defaultFixedFontSize(14)
    {
        //Initialize our defaults
        // changing any of those will likely break the LayoutTests
        fontFamilies[QWebSettings::StandardFont] = QLatin1String("Arial");
        fontFamilies[QWebSettings::FixedFont] = QLatin1String("Courier");
        fontFamilies[QWebSettings::SerifFont] = QLatin1String("Times New Roman");
        fontFamilies[QWebSettings::SansSerifFont] = QLatin1String("Arial");

        attributes[QWebSettings::AutoLoadImages]           = true;
        attributes[QWebSettings::JavascriptEnabled]        = true;
    }

    QHash<int, QString> fontFamilies;
    int minimumFontSize;
    int minimumLogicalFontSize;
    int defaultFontSize;
    int defaultFixedFontSize;
    QHash<int, bool> attributes;
    QHash<int, QPixmap> graphics;
    QString userStyleSheetLocation;
};

static QWebSettings globalSettings;

QWebSettings::QWebSettings()
    : d(new QWebSettingsPrivate)
{
}

QWebSettings::~QWebSettings()
{
}

void QWebSettings::setMinimumFontSize(int size)
{
    d->minimumFontSize = size;
}


int QWebSettings::minimumFontSize() const
{
    return d->minimumFontSize;
}


void QWebSettings::setMinimumLogicalFontSize(int size)
{
    d->minimumLogicalFontSize = size;
}


int QWebSettings::minimumLogicalFontSize() const
{
    return d->minimumLogicalFontSize;
}


void QWebSettings::setDefaultFontSize(int size)
{
    d->defaultFontSize = size;
}


int QWebSettings::defaultFontSize() const
{
    return d->defaultFontSize;
}


void QWebSettings::setDefaultFixedFontSize(int size)
{
    d->defaultFixedFontSize = size;
}


int QWebSettings::defaultFixedFontSize() const
{
    return d->defaultFixedFontSize;
}

void QWebSettings::setUserStyleSheetLocation(const QString &location)
{
    d->userStyleSheetLocation = location;
}

QString QWebSettings::userStyleSheetLocation() const
{
    return d->userStyleSheetLocation;
}

void QWebSettings::setIconDatabaseEnabled(bool enabled, const QString &location)
{
    WebCore::iconDatabase()->setEnabled(enabled);
    if (enabled) {
      if (!location.isEmpty()) {
          WebCore::iconDatabase()->open(location);
      } else {
          WebCore::iconDatabase()->open(WebCore::iconDatabase()->defaultDatabaseFilename());
      }
    } else {
      WebCore::iconDatabase()->close();
    }
}

bool QWebSettings::iconDatabaseEnabled() const
{
    return WebCore::iconDatabase()->isEnabled() && WebCore::iconDatabase()->isOpen();
}

void QWebSettings::setWebGraphic(WebGraphic type, const QPixmap &graphic)
{
    d->graphics[type] = graphic;
}

QPixmap QWebSettings::webGraphic(WebGraphic type) const
{
    if (d->graphics.contains(type))
      return d->graphics[type];
    else
      return QPixmap();
}

QWebSettings::QWebSettings(const QWebSettings &other)
{
    d = other.d;
}

QWebSettings &QWebSettings::operator=(const QWebSettings &other)
{
    d = other.d;
    return *this;
}

void QWebSettings::setGlobal(const QWebSettings &settings)
{
    globalSettings = settings;
}

QWebSettings QWebSettings::global()
{
    return globalSettings;
}

void QWebSettings::setFontFamily(FontType type, const QString &family)
{
    d->fontFamilies[type] = family;
}

QString QWebSettings::fontFamily(FontType type) const
{
    return d->fontFamilies[type];
}

void QWebSettings::setAttribute(WebAttribute attr, bool on)
{
    d->attributes[attr] = on;
}

bool QWebSettings::testAttribute(WebAttribute attr) const
{
    if (!d->attributes.contains(attr))
        return false;
    return d->attributes[attr];
}

QPixmap loadResourcePixmap(const char *name)
{
    const QWebSettings settings = QWebSettings::global();
    const QString resource = name;

    QPixmap pixmap;
    if (resource == "missingImage")
        pixmap = settings.webGraphic(QWebSettings::MissingImageGraphic);
    else if (resource == "nullPlugin")
        pixmap = settings.webGraphic(QWebSettings::MissingPluginGraphic);
    else if (resource == "urlIcon")
        pixmap = settings.webGraphic(QWebSettings::DefaultFaviconGraphic);
    else if (resource == "textAreaResizeCorner")
        pixmap = settings.webGraphic(QWebSettings::TextAreaResizeCornerGraphic);

    return pixmap;
}
