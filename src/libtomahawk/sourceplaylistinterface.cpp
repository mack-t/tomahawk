/* === This file is part of Tomahawk Player - <http://tomahawk-player.org> ===
 *
 *   Copyright 2010-2011, Christian Muehlhaeuser <muesli@tomahawk-player.org>
 *
 *   Tomahawk is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 *   Tomahawk is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with Tomahawk. If not, see <http://www.gnu.org/licenses/>.
 */

#include "sourceplaylistinterface.h"

#include <QDebug>

#include "source.h"
#include "pipeline.h"

using namespace Tomahawk;

SourcePlaylistInterface::SourcePlaylistInterface( Tomahawk::source_ptr& source )
    : PlaylistInterface( this )
    , m_source( source )
{
    connect( source.data(), SIGNAL( playbackStarted( const Tomahawk::query_ptr& ) ), SLOT( onSourcePlaybackStarted( const Tomahawk::query_ptr& ) ) );
}


Tomahawk::result_ptr
SourcePlaylistInterface::siblingItem( int itemsAway )
{
    Q_UNUSED( itemsAway );
    qDebug() << Q_FUNC_INFO;
    if ( m_source->currentTrack()->results().empty() )
    {
        qDebug() << Q_FUNC_INFO << " Results were empty for current track";
        return Tomahawk::result_ptr();
    }

    return m_source->currentTrack()->results().first();
}


Tomahawk::result_ptr
SourcePlaylistInterface::nextItem()
{
    qDebug() << Q_FUNC_INFO;
    if ( m_source->currentTrack()->results().empty() )
    {
        qDebug() << Q_FUNC_INFO << " Results were empty for current track";
        return Tomahawk::result_ptr();
    }

    return m_source->currentTrack()->results().first();
}


QList<Tomahawk::query_ptr>
SourcePlaylistInterface::tracks()
{
    return m_source->collection()->tracks();
}


void
SourcePlaylistInterface::onSourcePlaybackStarted( const Tomahawk::query_ptr& query ) const
{
    qDebug() << Q_FUNC_INFO;
    connect( query.data(), SIGNAL( resultsAdded( const QList<Tomahawk::result_ptr>& ) ), SLOT( resolveResultsAdded( const QList<Tomahawk::result_ptr>& ) ) );
    connect( query.data(), SIGNAL( resolvingFinished( bool ) ), SLOT( resolvingFinished( bool ) ) );
    Pipeline::instance()->resolve( query, true );
}


void
SourcePlaylistInterface::resolveResultsAdded( const QList<Tomahawk::result_ptr>& results ) const
{
    qDebug() << Q_FUNC_INFO;
    foreach ( Tomahawk::result_ptr ptr, results )
    {
        qDebug() << "Found result: " << ptr->track();
    }
}

void
SourcePlaylistInterface::resolvingFinished( bool hasResults ) const
{
    qDebug() << Q_FUNC_INFO << " and has results? : " << (hasResults ? "true" : "false");
}