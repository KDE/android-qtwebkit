/*
 *  Copyright (C) 2008 Nokia Corporation and/or its subsidiary(-ies)
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "config.h"
#include "MimeType.h"

#include "Frame.h"
#include "FrameLoaderClient.h"
#include "Page.h"
#include "Plugin.h"
#include "PluginData.h"
#include "Settings.h"

namespace WebCore {

MimeType::MimeType(PassRefPtr<PluginData> pluginData, unsigned index)
    : m_pluginData(pluginData)
    , m_index(index)
{
}

MimeType::~MimeType()
{
}

const String &MimeType::type() const
{
    return mimeClassInfo().type;
}

const String &MimeType::suffixes() const
{
    return mimeClassInfo().suffixes;
}

const String &MimeType::description() const
{
    return mimeClassInfo().desc;
}

PassRefPtr<Plugin> MimeType::enabledPlugin() const
{
    const Page* p = m_pluginData->page();
    if (!p || !p->mainFrame()->loader()->allowPlugins(NotAboutToInstantiatePlugin))
        return 0;

    return Plugin::create(m_pluginData.get(), mimeClassInfo().pluginIndex);
}

} // namespace WebCore
