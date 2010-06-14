/*
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */
#ifndef WEBPLUGIN_H
#define WEBPLUGIN_H

#include "qwebkitplatformplugin.h"
#include "WebNotificationPresenter.h"

#include <QDialog>

class QListWidgetItem;
class QListWidget;

class Popup : public QDialog {
    Q_OBJECT
public:
    Popup(const QWebSelectData& data) : m_data(data) { setModal(true); }

signals:
    void itemClicked(int idx);

protected slots:
    void onItemSelected(QListWidgetItem* item);

protected:
    void populateList();

    const QWebSelectData& m_data;
    QListWidget* m_list;
};


class SingleSelectionPopup : public Popup {
    Q_OBJECT
public:
    SingleSelectionPopup(const QWebSelectData& data);
};


class MultipleSelectionPopup : public Popup {
    Q_OBJECT
public:
    MultipleSelectionPopup(const QWebSelectData& data);
};


class WebPopup : public QWebSelectMethod {
    Q_OBJECT
public:
    WebPopup();
    ~WebPopup();

    virtual void show(const QWebSelectData& data);
    virtual void hide();

private slots:
    void popupClosed();
    void itemClicked(int idx);

private:
    Popup* m_popup;

    Popup* createPopup(const QWebSelectData& data);
    Popup* createSingleSelectionPopup(const QWebSelectData& data);
    Popup* createMultipleSelectionPopup(const QWebSelectData& data);
};

class WebPlugin : public QObject, public QWebKitPlatformPlugin
{
    Q_OBJECT
    Q_INTERFACES(QWebKitPlatformPlugin)
public:
    virtual QWebSelectMethod* createSelectInputMethod() const { return new WebPopup(); }
    virtual bool supportsExtension(Extension extension) const;
    virtual QWebNotificationPresenter* createNotificationPresenter() const {
        return new WebNotificationPresenter();
    }
};

#endif // WEBPLUGIN_H
