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

#ifndef SOURCELIST_H
#define SOURCELIST_H

#include <QObject>
#include <QMutex>
#include <QMap>

#include "typedefs.h"
#include "source.h"

#include "dllmacro.h"

class DLLEXPORT SourceList : public QObject
{
Q_OBJECT

public:
    static SourceList* instance();

    explicit SourceList( QObject* parent = 0 );

    bool isReady() const { return m_isReady; }

    const Tomahawk::source_ptr& getLocal();
    void setLocal( const Tomahawk::source_ptr& localSrc );

    void setWebSource( const Tomahawk::source_ptr& websrc );
    const Tomahawk::source_ptr webSource() const;

    void loadSources();
    void removeAllRemote();

    QList<Tomahawk::source_ptr> sources( bool onlyOnline = false ) const;
    unsigned int count() const;

    Tomahawk::source_ptr get( const QString& username, const QString& friendlyName = QString() );
    Tomahawk::source_ptr get( int id ) const;

signals:
    void ready();

    void sourceAdded( const Tomahawk::source_ptr& );
    void sourceRemoved( const Tomahawk::source_ptr& );

    void sourceLatchedOn( const Tomahawk::source_ptr& from, const Tomahawk::source_ptr& to );
    void sourceLatchedOff( const Tomahawk::source_ptr& from, const Tomahawk::source_ptr& to );

private slots:
    void setSources( const QList<Tomahawk::source_ptr>& sources );
    void sourceSynced();

    void latchedOn( const Tomahawk::source_ptr& );
    void latchedOff( const Tomahawk::source_ptr& );

private:
    void add( const Tomahawk::source_ptr& source );

    QMap< QString, Tomahawk::source_ptr > m_sources;
    QMap< int, QString > m_sources_id2name;

    bool m_isReady;
    Tomahawk::source_ptr m_local;
    Tomahawk::source_ptr m_dummy;
    mutable QMutex m_mut; // mutable so const methods can use a lock

    static SourceList* s_instance;
};

#endif // SOURCELIST_H
