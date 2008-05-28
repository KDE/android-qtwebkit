/*
    Copyright (C) 2008 Trolltech ASA

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
#include "MediaPlayerPrivatePhonon.h"

#include <limits>

#include "CString.h"
#include "FrameView.h"
#include "GraphicsContext.h"
#include "NotImplemented.h"
#include "Widget.h"
#include <wtf/HashSet.h>

#include <QDebug>
#include <QPainter>
#include <QWidget>
#include <QMetaEnum>
#include <QUrl>
#include <QEvent>
#include <phonon>

using namespace Phonon;

#define LOG_MEDIAOBJECT() (LOG(Media,"%s", debugMediaObject(*m_mediaObject).constData()))

static QByteArray debugMediaObject(const MediaObject& mediaObject)
{
    QByteArray byteArray;
    QTextStream stream(&byteArray);
    
    const QMetaObject* metaObject = mediaObject.metaObject();
    QMetaEnum phononStates = metaObject->enumerator(metaObject->indexOfEnumerator("PhononState"));

    stream << "Phonon::MediaObject(";
    stream << "State:" << phononStates.valueToKey(mediaObject.state());
    stream << "| Current time:" << mediaObject.currentTime();
    stream << "| Remaining time:" << mediaObject.remainingTime();
    stream << "| Total time:" << mediaObject.totalTime();
    stream << "| Meta-data:";
    QMultiMap<QString, QString> map = mediaObject.metaData();
    for (QMap<QString, QString>::const_iterator it = map.constBegin();
        it != map.constEnd(); ++it) {
        stream << "(" << it.key() << ", " << it.value() << ")";
    }
    stream << "| Has video:" << mediaObject.hasVideo();
    stream << "| Is seekable:" << mediaObject.isSeekable();
    stream << ")";
    
    stream.flush();

    return byteArray;
}

using namespace WTF;

namespace WebCore {

MediaPlayerPrivate::MediaPlayerPrivate(MediaPlayer* player)
    : m_player(player)
    , m_networkState(MediaPlayer::Empty)
    , m_readyState(MediaPlayer::DataUnavailable)
    , m_mediaObject(new MediaObject())
    , m_videoWidget(new VideoWidget(0))
    , m_audioOutput(new AudioOutput())
    , m_isVisible(false)
{
    // Hint to Phonon to disable overlay painting
    m_videoWidget->setAttribute(Qt::WA_DontShowOnScreen);

    createPath(m_mediaObject, m_videoWidget);
    createPath(m_mediaObject, m_audioOutput);

    // Make sure we get updates for each frame
    m_videoWidget->installEventFilter(this);
    foreach(QWidget* widget, qFindChildren<QWidget*>(m_videoWidget)) {
        widget->installEventFilter(this);
    }

    connect(m_mediaObject, SIGNAL(stateChanged(Phonon::State, Phonon::State)),
            this, SLOT(stateChanged(Phonon::State, Phonon::State)));
    connect(m_mediaObject, SIGNAL(tick(qint64)), this, SLOT(tick(qint64)));
    connect(m_mediaObject, SIGNAL(metaDataChanged()), this, SLOT(metaDataChanged()));
    connect(m_mediaObject, SIGNAL(seekableChanged(bool)), this, SLOT(seekableChanged(bool)));
    connect(m_mediaObject, SIGNAL(hasVideoChanged(bool)), this, SLOT(hasVideoChanged(bool)));
    connect(m_mediaObject, SIGNAL(bufferStatus(int)), this, SLOT(bufferStatus(int)));
    connect(m_mediaObject, SIGNAL(finished()), this, SLOT(finished()));
    connect(m_mediaObject, SIGNAL(currentSourceChanged(const Phonon::MediaSource&)),
            this, SLOT(currentSourceChanged(const Phonon::MediaSource&)));
    connect(m_mediaObject, SIGNAL(aboutToFinish()), this, SLOT(aboutToFinish()));
    connect(m_mediaObject, SIGNAL(prefinishMarkReached(qint32)), this, SLOT(prefinishMarkReached(qint32)));
    connect(m_mediaObject, SIGNAL(totalTimeChanged(qint64)), this, SLOT(totalTimeChanged(qint64)));
}

MediaPlayerPrivate::~MediaPlayerPrivate()
{
    LOG(Media, "MediaPlayerPrivatePhonon::dtor deleting videowidget");
    m_videoWidget->close();
    delete m_videoWidget;
    m_videoWidget = 0;

    LOG(Media, "MediaPlayerPrivatePhonon::dtor deleting audiooutput");
    delete m_audioOutput;
    m_audioOutput = 0;

    LOG(Media, "MediaPlayerPrivatePhonon::dtor deleting mediaobject");
    delete m_mediaObject;
    m_mediaObject = 0;
}

void MediaPlayerPrivate::getSupportedTypes(HashSet<String>&)
{
    notImplemented();
}

bool MediaPlayerPrivate::hasVideo() const
{
    bool hasVideo = m_mediaObject->hasVideo();
    LOG(Media, "MediaPlayerPrivatePhonon::hasVideo() -> %s", hasVideo ? "true" : "false");
    return hasVideo;
}

void MediaPlayerPrivate::load(String url)
{
    LOG(Media, "MediaPlayerPrivatePhonon::load(\"%s\")", url.utf8().data());

    // We are now loading
    if (m_networkState != MediaPlayer::Loading) {
        m_networkState = MediaPlayer::Loading;
        m_player->networkStateChanged();
    }

    // And we don't have any data yet
    if (m_readyState != MediaPlayer::DataUnavailable) {
        m_readyState = MediaPlayer::DataUnavailable;
        m_player->readyStateChanged();
    }

    m_mediaObject->setCurrentSource(QUrl(url));
    m_audioOutput->setVolume(m_player->volume());
    setVisible(m_player->visible());
}

void MediaPlayerPrivate::cancelLoad()
{
    notImplemented();
}


void MediaPlayerPrivate::play()
{
    Q_ASSERT(m_mediaObject);

    LOG(Media, "MediaPlayerPrivatePhonon::play()");
    m_mediaObject->play();
}

void MediaPlayerPrivate::pause()
{
    Q_ASSERT(m_mediaObject);

    LOG(Media, "MediaPlayerPrivatePhonon::pause()");
    m_mediaObject->pause();
}


bool MediaPlayerPrivate::paused() const
{
    Q_ASSERT(m_mediaObject);

    bool paused = m_mediaObject->state() == Phonon::PausedState;
    LOG(Media, "MediaPlayerPrivatePhonon::paused() --> %s", paused ? "true" : "false");
    return paused;
}

void MediaPlayerPrivate::seek(float)
{
    notImplemented();
}

bool MediaPlayerPrivate::seeking() const
{
    notImplemented();
    return false;
}

float MediaPlayerPrivate::duration() const
{
    Q_ASSERT(m_mediaObject);

    if (m_networkState < MediaPlayer::LoadedMetaData)
        return 0.0f;

    float duration = m_mediaObject->totalTime() / 1000.0f;

    if (duration == 0.0f) // We are streaming
        duration = std::numeric_limits<float>::infinity();

    LOG(Media, "MediaPlayerPrivatePhonon::duration() --> %f", duration);
    return duration;
}

float MediaPlayerPrivate::currentTime() const
{
    Q_ASSERT(m_mediaObject);

    float currentTime = m_mediaObject->currentTime() / 1000.0f;

    LOG(Media, "MediaPlayerPrivatePhonon::currentTime() --> %f", currentTime);
    return currentTime;
}

void MediaPlayerPrivate::setEndTime(float endTime)
{
    notImplemented();
}

float MediaPlayerPrivate::maxTimeBuffered() const
{
    notImplemented();
    return 0.0f;
}

float MediaPlayerPrivate::maxTimeSeekable() const
{
    notImplemented();
    return 0.0f;
}

unsigned MediaPlayerPrivate::bytesLoaded() const
{ 
    notImplemented();
    return 0;
}

bool MediaPlayerPrivate::totalBytesKnown() const
{
    //notImplemented();
    return false;
}

unsigned MediaPlayerPrivate::totalBytes() const
{
    //notImplemented();
    return 0;
}

void MediaPlayerPrivate::setRate(float)
{
    notImplemented();
}

void MediaPlayerPrivate::setVolume(float volume)
{
    Q_ASSERT(m_audioOutput);

    m_audioOutput->setVolume(volume);
}

void MediaPlayerPrivate::setMuted(bool muted)
{
    Q_ASSERT(m_audioOutput);

    m_audioOutput->setMuted(muted);
}


int MediaPlayerPrivate::dataRate() const
{
    notImplemented();
    return 0;
}


MediaPlayer::NetworkState MediaPlayerPrivate::networkState() const
{
    const QMetaObject* metaObj = this->metaObject();
    QMetaEnum networkStates = metaObj->enumerator(metaObj->indexOfEnumerator("NetworkState"));
    LOG(Media, "MediaPlayerPrivatePhonon::networkState() --> %s", networkStates.valueToKey(m_networkState));
    return m_networkState;
}

MediaPlayer::ReadyState MediaPlayerPrivate::readyState() const
{
    const QMetaObject* metaObj = this->metaObject();
    QMetaEnum readyStates = metaObj->enumerator(metaObj->indexOfEnumerator("ReadyState"));
    LOG(Media, "MediaPlayerPrivatePhonon::readyState() --> %s", readyStates.valueToKey(m_readyState));
    return m_readyState;
}

void MediaPlayerPrivate::updateStates()
{
    Q_ASSERT(m_mediaObject);

    MediaPlayer::NetworkState oldNetworkState = m_networkState;
    MediaPlayer::ReadyState oldReadyState = m_readyState;

    Phonon::State state = m_mediaObject ? m_mediaObject->state() : Phonon::ErrorState;

    if (state == Phonon::StoppedState) {
        if (oldNetworkState < MediaPlayer::LoadedMetaData) {
            m_networkState = MediaPlayer::LoadedMetaData;
            m_readyState = MediaPlayer::DataUnavailable;
            m_mediaObject->pause();
        }
    } else if (state == Phonon::PausedState) {
        m_networkState = MediaPlayer::LoadedFirstFrame;
        m_readyState = MediaPlayer::CanPlayThrough;
    }

    if (seeking())
        m_readyState = MediaPlayer::DataUnavailable;

    if (m_networkState != oldNetworkState) {
        const QMetaObject* metaObj = this->metaObject();
        QMetaEnum networkStates = metaObj->enumerator(metaObj->indexOfEnumerator("NetworkState"));
        LOG(Media, "Network state changed from '%s' to '%s'",
                networkStates.valueToKey(oldNetworkState),
                networkStates.valueToKey(m_networkState));
        m_player->networkStateChanged();
    }

    if (m_readyState != oldReadyState) {
        const QMetaObject* metaObj = this->metaObject();
        QMetaEnum readyStates = metaObj->enumerator(metaObj->indexOfEnumerator("ReadyState"));
        LOG(Media, "Ready state changed from '%s' to '%s'",
                readyStates.valueToKey(oldReadyState),
                readyStates.valueToKey(m_readyState));
        m_player->readyStateChanged();
    }
}

void MediaPlayerPrivate::setVisible(bool visible)
{
    m_isVisible = visible;
    LOG(Media, "MediaPlayerPrivatePhonon::setVisible(%s)", visible ? "true" : "false");

    m_videoWidget->setVisible(m_isVisible);
}

void MediaPlayerPrivate::setRect(const IntRect& newRect)
{
    if (!m_videoWidget)
        return;

    LOG(Media, "MediaPlayerPrivatePhonon::setRect(%d,%d %dx%d)",
                newRect.x(), newRect.y(),
                newRect.width(), newRect.height());

    QRect currentRect = m_videoWidget->rect();

    if (newRect.width() != currentRect.width() || newRect.height() != currentRect.height())
        m_videoWidget->resize(newRect.width(), newRect.height());
}


void MediaPlayerPrivate::loadStateChanged() 
{
    notImplemented();
}

void MediaPlayerPrivate::rateChanged()
{
    notImplemented();
}

void MediaPlayerPrivate::sizeChanged()
{
    notImplemented();
}

void MediaPlayerPrivate::timeChanged()
{
    notImplemented();
}

void MediaPlayerPrivate::volumeChanged()
{
    notImplemented();
}

void MediaPlayerPrivate::didEnd()
{
    notImplemented();
}

void MediaPlayerPrivate::loadingFailed()
{
    notImplemented();
}

IntSize MediaPlayerPrivate::naturalSize() const
{
    Q_ASSERT(m_videoWidget);

    if (!hasVideo())
         return IntSize();

    if (m_networkState < MediaPlayer::LoadedMetaData)
           return IntSize();

    QSize videoSize = m_videoWidget->sizeHint();
    IntSize naturalSize(videoSize.width(), videoSize.height());
    LOG(Media, "MediaPlayerPrivatePhonon::naturalSize() -> %dx%d",
            naturalSize.width(), naturalSize.height());
    return naturalSize;
}

bool MediaPlayerPrivate::eventFilter(QObject* obj, QEvent* event)
{
    if (event->type() == QEvent::Paint)
        m_player->repaint();

    return QObject::eventFilter(obj, event);
}

void MediaPlayerPrivate::repaint()
{
    m_player->repaint();
}

void MediaPlayerPrivate::paint(GraphicsContext* graphicsContect, const IntRect& rect)
{
    Q_ASSERT(m_videoWidget);

    if (graphicsContect->paintingDisabled())
        return;

    if (!m_isVisible)
        return;

    //QPainter* painter = static_cast<QPainter*>(graphicsContect->platformContext());
    QPainter* painter = graphicsContect->platformContext();

    painter->fillRect(rect, Qt::black);

    m_videoWidget->render(painter, QPoint(rect.x(), rect.y()),
            QRegion(0, 0, rect.width(), rect.height()));
}

// ====================== Phonon::MediaObject signals ======================

void MediaPlayerPrivate::stateChanged(Phonon::State, Phonon::State)
{
    notImplemented();
    LOG_MEDIAOBJECT();
    updateStates();
}

void MediaPlayerPrivate::tick(qint64)
{
    notImplemented();
}

void MediaPlayerPrivate::metaDataChanged()
{
    LOG(Media, "MediaPlayerPrivatePhonon::metaDataChanged()");
    LOG_MEDIAOBJECT();
}

void MediaPlayerPrivate::seekableChanged(bool)
{
    notImplemented();
    LOG_MEDIAOBJECT();
}

void MediaPlayerPrivate::hasVideoChanged(bool)
{
    notImplemented();
    LOG_MEDIAOBJECT();
}

void MediaPlayerPrivate::bufferStatus(int)
{
    notImplemented();
    LOG_MEDIAOBJECT();
}

void MediaPlayerPrivate::finished()
{
    notImplemented();
    LOG_MEDIAOBJECT();
}

void MediaPlayerPrivate::currentSourceChanged(const Phonon::MediaSource&)
{
    notImplemented();
    LOG_MEDIAOBJECT();
}

void MediaPlayerPrivate::aboutToFinish()
{
    notImplemented();
    LOG_MEDIAOBJECT();
}

void MediaPlayerPrivate::prefinishMarkReached(qint32)
{
    notImplemented();
    LOG_MEDIAOBJECT();
}

void MediaPlayerPrivate::totalTimeChanged(qint64 totalTime)
{
    LOG(Media, "MediaPlayerPrivatePhonon::totalTimeChanged(%d)", totalTime);
    LOG_MEDIAOBJECT();
}

} // namespace WebCore

#include "moc_MediaPlayerPrivatePhonon.cpp"
