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
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with Tomahawk. If not, see <http://www.gnu.org/licenses/>.
 */

#include "logger.h"

#include <iostream>
#include <fstream>

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QMutex>
#include <QTime>
#include <QVariant>

#include "utils/tomahawkutils.h"

#define LOGFILE TomahawkUtils::appLogDir().filePath( "Tomahawk.log" ).toLocal8Bit()
#define LOGFILE_SIZE 1024 * 256

#define RELEASE_LEVEL_THRESHOLD 0
#define DEBUG_LEVEL_THRESHOLD LOGEXTRA

using namespace std;
ofstream logfile;
static int s_threshold = -1;

namespace Logger
{

static void
log( const char *msg, unsigned int debugLevel, bool toDisk = true )
{
    if ( s_threshold < 0 )
    {
        if ( qApp->arguments().contains( "--verbose" ) )
            s_threshold = LOGTHIRDPARTY;
        else
            #ifdef QT_NO_DEBUG
            s_threshold = RELEASE_LEVEL_THRESHOLD;
            #else
            s_threshold = DEBUG_LEVEL_THRESHOLD;
            #endif
    }

    #ifdef QT_NO_DEBUG
    if ( debugLevel > RELEASE_LEVEL_THRESHOLD )
        toDisk = false;
    #else
    if ( debugLevel > DEBUG_LEVEL_THRESHOLD )
        toDisk = false;
    #endif

    if ( toDisk || (int)debugLevel <= s_threshold )
    {
        logfile << QTime::currentTime().toString().toAscii().data() << " [" << QString::number( debugLevel ).toAscii().data() << "]: " << msg << endl;
        logfile.flush();
    }

    if ( debugLevel <= LOGEXTRA || (int)debugLevel <= s_threshold )
    {
        cout << msg << endl;
        cout.flush();
    }
}


void
TomahawkLogHandler( QtMsgType type, const char *msg )
{
    static QMutex s_mutex;

    QMutexLocker locker( &s_mutex );
    switch( type )
    {
        case QtDebugMsg:
            log( msg, LOGTHIRDPARTY );
            break;

        case QtCriticalMsg:
            log( msg, 0 );
            break;

        case QtWarningMsg:
            log( msg, 0 );
            break;

        case QtFatalMsg:
            log( msg, 0 );
            break;
    }
}


void
setupLogfile()
{
    if ( QFileInfo( LOGFILE ).size() > LOGFILE_SIZE )
    {
        QByteArray lc;
        {
            QFile f( LOGFILE );
            f.open( QIODevice::ReadOnly | QIODevice::Text );
            lc = f.readAll();
            f.close();
        }

        QFile::remove( LOGFILE );

        {
            QFile f( LOGFILE );
            f.open( QIODevice::WriteOnly | QIODevice::Text );
            f.write( lc.right( LOGFILE_SIZE - (LOGFILE_SIZE / 4) ) );
            f.close();
        }
    }

    logfile.open( LOGFILE, ios::app );
    qInstallMsgHandler( TomahawkLogHandler );
}

}

using namespace Logger;

TLog::TLog( unsigned int debugLevel )
    : QDebug( &m_msg )
    , m_debugLevel( debugLevel )
{
}


TLog::~TLog()
{
    log( m_msg.toAscii().data(), m_debugLevel );
}

