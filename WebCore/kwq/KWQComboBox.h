/*
 * Copyright (C) 2004 Apple Computer, Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#ifndef QCOMBOBOX_H_
#define QCOMBOBOX_H_

#include "KWQListBox.h"

#ifdef __OBJC__
@class KWQComboBoxAdapter;
#else
class KWQComboBoxAdapter;
class NSMenuItem;
#endif

class QComboBox : public QWidget {
public:  
    QComboBox();
    ~QComboBox();
    
    void clear();
    void appendItem(const QString &text) { appendItem(text, KWQListBoxOption); }
    void appendGroupLabel(const QString &text) { appendItem(text, KWQListBoxGroupLabel); }
    void appendSeparator() { appendItem(QString::null, KWQListBoxSeparator); }

    int currentItem() const { return _currentItem; }
    void setCurrentItem(int);

    QListBox *listBox() const { return 0; }
    void popup() { }
    
    QSize sizeHint() const;
    QRect frameGeometry() const;
    void setFrameGeometry(const QRect &);
    int baselinePosition(int height) const;
    void setFont(const QFont &);

    void itemSelected();
    
    virtual FocusPolicy focusPolicy() const;

    void setWritingDirection(QPainter::TextDirection);

    virtual void populate();
    void populateMenu();
    
private:
    void appendItem(const QString &, KWQListBoxItemType);
    const int *dimensions() const;
    NSFont *labelFont() const;
    void setTitle(NSMenuItem *, const KWQListBoxItem &);
    
    mutable int _width;
    mutable bool _widthGood;

    mutable int _currentItem;

    // A vector<KWQListBoxItem> or QValueVector<KWQListBoxItem> may be more efficient for large menus.
    QValueList<KWQListBoxItem> _items;
    mutable bool _menuPopulated;

    mutable NSFont *_labelFont;

    KWQSignal _activated;
};

#endif
