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

#include "tomahawkdesktopwindow.h"
#include "ui_tomahawkdesktopwindow.h"

#include <QAction>
#include <QCloseEvent>
#include <QShowEvent>
#include <QHideEvent>
#include <QInputDialog>
#include <QPixmap>
#include <QPropertyAnimation>
#include <QLineEdit>
#include <QMessageBox>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QTimer>
#include <QToolBar>

#include "playlist.h"
#include "query.h"
#include "artist.h"

#include "audio/audioengine.h"
#include "viewmanager.h"
#include "sip/SipHandler.h"
#include "sourcetree/sourcetreeview.h"
#include "widgets/animatedsplitter.h"
#include "utils/proxystyle.h"
#include "utils/widgetdragfilter.h"
#include "utils/xspfloader.h"
#include "widgets/newplaylistwidget.h"
#include "widgets/searchwidget.h"
#include "widgets/playlisttypeselectordlg.h"

#include "audiocontrols.h"
#include "settingsdialog.h"
#include "diagnosticsdialog.h"
#include "tomahawksettings.h"
#include "sourcelist.h"
#include "PipelineStatusView.h"
#include "transferview.h"
#include "tomahawktrayicon.h"
#include "playlist/dynamic/GeneratorInterface.h"
#include "scanmanager.h"
#include "tomahawkapp.h"

#ifdef Q_OS_WIN32
#include <qtsparkle/Updater>
#endif

#include "utils/logger.h"
#include "thirdparty/Qocoa/qsearchfield.h"

using namespace Tomahawk;


TomahawkDesktopWindow::TomahawkDesktopWindow( QWidget* parent )
    : TomahawkWindow( parent )
    , ui( new Ui::TomahawkDesktopWindow )
    , m_searchWidget( 0 )
    , m_audioControls( new AudioControls( this ) )
{
    ViewManager* vm = new ViewManager( this );
    connect( vm, SIGNAL( showQueueRequested() ), SLOT( showQueue() ) );
    connect( vm, SIGNAL( hideQueueRequested() ), SLOT( hideQueue() ) );

    ui->setupUi( this );
    applyPlatformTweaks();

    ui->centralWidget->setContentsMargins( 0, 0, 0, 0 );
    TomahawkUtils::unmarginLayout( ui->centralWidget->layout() );

    setupSideBar();
    statusBar()->addPermanentWidget( m_audioControls, 1 );

    setupUpdateCheck();
    loadSettings();
    setupSignals();

    // set initial state
    onSipDisconnected();
    vm->setQueue( m_queueView );
    ViewManager::instance()->showWelcomePage();
}


TomahawkDesktopWindow::~TomahawkDesktopWindow()
{
    delete ui;
}


void
TomahawkDesktopWindow::retranslateUi()
{
    ui->retranslateUi( this );
}


void
TomahawkDesktopWindow::loadSettings()
{
    TomahawkSettings* s = TomahawkSettings::instance();

    if ( !s->mainWindowSplitterState().isEmpty() )
        ui->splitter->restoreState( s->mainWindowSplitterState() );

    TomahawkWindow::loadSettings();
}


void
TomahawkDesktopWindow::saveSettings()
{
    TomahawkSettings* s = TomahawkSettings::instance();
    s->setMainWindowSplitterState( ui->splitter->saveState() );

    TomahawkWindow::saveSettings();
}


void
TomahawkDesktopWindow::applyPlatformTweaks()
{
    // HACK QtCurve causes an infinite loop on startup. This is because setStyle calls setPalette, which calls ensureBaseStyle,
    // which loads QtCurve. QtCurve calls setPalette, which creates an infinite loop. The UI will look like CRAP with QtCurve, but
    // the user is asking for it explicitly... so he's gonna be stuck with an ugly UI.
    if ( !QString( qApp->style()->metaObject()->className() ).toLower().contains( "qtcurve" ) )
        qApp->setStyle( new ProxyStyle() );

#ifdef Q_OS_MAC
    setUnifiedTitleAndToolBarOnMac( true );
    delete ui->hline1;
    delete ui->hline2;
#else
    ui->hline1->setStyleSheet( "border: 1px solid gray;" );
    ui->hline2->setStyleSheet( "border: 1px solid gray;" );
#endif
}


void
TomahawkDesktopWindow::setupSideBar()
{
    // Delete fake designer widgets
    delete ui->sidebarWidget;
    delete ui->playlistWidget;

    QWidget* sidebarWidget = new QWidget();
    sidebarWidget->setLayout( new QVBoxLayout() );

    m_sidebar = new AnimatedSplitter();
    m_sidebar->setOrientation( Qt::Vertical );
    m_sidebar->setChildrenCollapsible( false );

    m_searchWidget = new QSearchField( m_sidebar );
    m_searchWidget->setPlaceholderText( "Global Search..." );
    connect( m_searchWidget, SIGNAL( returnPressed() ), this, SLOT( onFilterEdited() ) );

    m_sourcetree = new SourceTreeView();
    TransferView* transferView = new TransferView( m_sidebar );
    PipelineStatusView* pipelineView = new PipelineStatusView( m_sidebar );

    m_queueView = new QueueView( m_sidebar );
    m_queueModel = new PlaylistModel( m_queueView );
    m_queueModel->setStyle( PlaylistModel::Short );
    m_queueView->queue()->setPlaylistModel( m_queueModel );
    m_queueView->queue()->playlistModel()->setReadOnly( false );
    AudioEngine::instance()->setQueue( m_queueView->queue()->proxyModel() );

    m_sidebar->addWidget( m_searchWidget );
    m_sidebar->addWidget( m_sourcetree );
    m_sidebar->addWidget( transferView );
    m_sidebar->addWidget( pipelineView );
    m_sidebar->addWidget( m_queueView );

    m_sidebar->setGreedyWidget( 1 );
    m_sidebar->hide( 1, false );
    m_sidebar->hide( 2, false );
    m_sidebar->hide( 3, false );
    m_sidebar->hide( 4, false );

    sidebarWidget->layout()->addWidget( m_sidebar );
    sidebarWidget->setContentsMargins( 0, 0, 0, 0 );
    sidebarWidget->layout()->setContentsMargins( 0, 0, 0, 0 );
    sidebarWidget->layout()->setMargin( 0 );

#ifndef Q_OS_MAC
    sidebarWidget->layout()->setSpacing( 0 );
#endif

    ui->splitter->addWidget( sidebarWidget );
    ui->splitter->addWidget( ViewManager::instance()->widget() );

    ui->splitter->setStretchFactor( 0, 1 );
    ui->splitter->setStretchFactor( 1, 3 );
    ui->splitter->setCollapsible( 1, false );
    ui->splitter->setHandleWidth( 1 );

    ui->actionShowOfflineSources->setChecked( TomahawkSettings::instance()->showOfflineSources() );
}

void
TomahawkDesktopWindow::setupUpdateCheck()
{
#ifndef Q_WS_MAC
    ui->menu_Help->insertSeparator( ui->actionAboutTomahawk );
#endif

#if defined( Q_OS_DARWIN ) && defined( HAVE_SPARKLE )
    QAction* checkForUpdates = ui->menu_Help->addAction( tr( "Check For Updates..." ) );
    checkForUpdates->setMenuRole( QAction::ApplicationSpecificRole );
    connect( checkForUpdates, SIGNAL( triggered( bool ) ), SLOT( checkForUpdates() ) );
#elif defined( WIN32 )
    QUrl updaterUrl;

    if ( qApp->arguments().contains( "--debug" ) )
        updaterUrl.setUrl( "http://download.tomahawk-player.org/sparklewin-debug" );
    else
        updaterUrl.setUrl( "http://download.tomahawk-player.org/sparklewin" );

    qtsparkle::Updater* updater = new qtsparkle::Updater( updaterUrl, this );
    Q_ASSERT( TomahawkUtils::nam() != 0 );
    updater->SetNetworkAccessManager( TomahawkUtils::nam() );
    updater->SetVersion( TomahawkUtils::appFriendlyVersion() );

    ui->menu_Help->addSeparator();
    QAction* checkForUpdates = ui->menu_Help->addAction( tr( "Check For Updates..." ) );
    connect( checkForUpdates, SIGNAL( triggered() ), updater, SLOT( CheckNow() ) );
#endif
}


void
TomahawkDesktopWindow::setupSignals()
{
    // <From PlaylistManager>
    connect( ViewManager::instance(), SIGNAL( repeatModeChanged( Tomahawk::PlaylistInterface::RepeatMode ) ),
             m_audioControls,           SLOT( onRepeatModeChanged( Tomahawk::PlaylistInterface::RepeatMode ) ) );
    connect( ViewManager::instance(), SIGNAL( shuffleModeChanged( bool ) ),
             m_audioControls,           SLOT( onShuffleModeChanged( bool ) ) );

    // <From AudioEngine>
    connect( AudioEngine::instance(), SIGNAL( loading( const Tomahawk::result_ptr& ) ),
             SLOT( onPlaybackLoading( const Tomahawk::result_ptr& ) ) );
    connect( AudioEngine::instance(), SIGNAL( started( Tomahawk::result_ptr ) ), SLOT( audioStarted() ) );
    connect( AudioEngine::instance(), SIGNAL( resumed()), SLOT( audioStarted() ) );
    connect( AudioEngine::instance(), SIGNAL( paused() ), SLOT( audioStopped() ) );
    connect( AudioEngine::instance(), SIGNAL( stopped() ), SLOT( audioStopped() ) );

    // <Menu Items>
    //    connect( ui->actionAddPeerManually, SIGNAL( triggered() ), SLOT( addPeerManually() ) );
    connect( ui->actionPreferences, SIGNAL( triggered() ), SLOT( showSettingsDialog() ) );
    connect( ui->actionDiagnostics, SIGNAL( triggered() ), SLOT( showDiagnosticsDialog() ) );
    connect( ui->actionToggleConnect, SIGNAL( triggered() ), SipHandler::instance(), SLOT( toggleConnect() ) );
    connect( ui->actionUpdateCollection, SIGNAL( triggered() ), SLOT( updateCollectionManually() ) );
    connect( ui->actionRescanCollection, SIGNAL( triggered() ), SLOT( rescanCollectionManually() ) );
    connect( ui->actionLoadXSPF, SIGNAL( triggered() ), SLOT( loadSpiff() ));
    connect( ui->actionCreatePlaylist, SIGNAL( triggered() ), SLOT( createPlaylist() ));
    connect( ui->actionCreate_New_Station, SIGNAL( triggered() ), SLOT( createStation() ));
    connect( ui->actionAboutTomahawk, SIGNAL( triggered() ), SLOT( showAboutTomahawk() ) );
    connect( ui->actionExit, SIGNAL( triggered() ), qApp, SLOT( quit() ) );
    connect( ui->actionShowOfflineSources, SIGNAL( triggered() ), SLOT( showOfflineSources() ) );

    connect( ui->actionPlay, SIGNAL( triggered() ), AudioEngine::instance(), SLOT( playPause() ) );
    connect( ui->actionNext, SIGNAL( triggered() ), AudioEngine::instance(), SLOT( next() ) );
    connect( ui->actionPrevious, SIGNAL( triggered() ), AudioEngine::instance(), SLOT( previous() ) );

    #if defined( Q_WS_MAC )
    connect( ui->actionMinimize, SIGNAL( triggered() ), SLOT( minimize() ) );
    connect( ui->actionZoom, SIGNAL( triggered() ), SLOT( maximize() ) );
    #else
    ui->menuWindow->clear();
    ui->menuWindow->menuAction()->setVisible( false );
    #endif

    // <SipHandler>
    connect( SipHandler::instance(), SIGNAL( connected( SipPlugin* ) ), SLOT( onSipConnected() ) );
    connect( SipHandler::instance(), SIGNAL( disconnected( SipPlugin* ) ), SLOT( onSipDisconnected() ) );
    connect( SipHandler::instance(), SIGNAL( authError( SipPlugin* ) ), SLOT( onSipError() ) );

    // <SipMenu>
    connect( SipHandler::instance(), SIGNAL( pluginAdded( SipPlugin* ) ), this, SLOT( onSipPluginAdded( SipPlugin* ) ) );
    connect( SipHandler::instance(), SIGNAL( pluginRemoved( SipPlugin* ) ), this, SLOT( onSipPluginRemoved( SipPlugin* ) ) );
    foreach( SipPlugin *plugin, SipHandler::instance()->allPlugins() )
    {
        connect( plugin, SIGNAL( addMenu( QMenu* ) ), this, SLOT( pluginMenuAdded( QMenu* ) ) );
        connect( plugin, SIGNAL( removeMenu( QMenu* ) ), this, SLOT( pluginMenuRemoved( QMenu* ) ) );
    }
}



void
TomahawkDesktopWindow::showSettingsDialog()
{
    qDebug() << Q_FUNC_INFO;
    SettingsDialog win;
    win.exec();
}


void TomahawkDesktopWindow::showDiagnosticsDialog()
{
    qDebug() << Q_FUNC_INFO;
    DiagnosticsDialog win;
    win.exec();
}


void
TomahawkDesktopWindow::updateCollectionManually()
{
    if ( TomahawkSettings::instance()->hasScannerPaths() )
        ScanManager::instance()->runScan();
}


void
TomahawkDesktopWindow::rescanCollectionManually()
{
    if ( TomahawkSettings::instance()->hasScannerPaths() )
        ScanManager::instance()->runScan( true );
}


void
TomahawkDesktopWindow::addPeerManually()
{
    TomahawkSettings* s = TomahawkSettings::instance();
    bool ok;
    QString addr = QInputDialog::getText( this, tr( "Connect To Peer" ),
                                                tr( "Enter peer address:" ), QLineEdit::Normal,
                                                s->value( "connip" ).toString(), &ok ); // FIXME
    if ( !ok )
        return;

    s->setValue( "connip", addr );
    QString ports = QInputDialog::getText( this, tr( "Connect To Peer" ),
                                                 tr( "Enter peer port:" ), QLineEdit::Normal,
                                                 s->value( "connport", "50210" ).toString(), &ok );
    if ( !ok )
        return;

    s->setValue( "connport", ports );
    int port = ports.toInt();
    QString key = QInputDialog::getText( this, tr( "Connect To Peer" ),
                                               tr( "Enter peer key:" ), QLineEdit::Normal,
                                               "whitelist", &ok );
    if ( !ok )
        return;

    qDebug() << "Attempting to connect to" << addr;
    Servent::instance()->connectToPeer( addr, port, key );
}


void
TomahawkDesktopWindow::pluginMenuAdded( QMenu* menu )
{
    ui->menuNetwork->addMenu( menu );
}


void
TomahawkDesktopWindow::pluginMenuRemoved( QMenu* menu )
{
    foreach( QAction* action, ui->menuNetwork->actions() )
    {
        if( action->menu() == menu )
        {
            ui->menuNetwork->removeAction( action );
            return;
        }
    }
}

void
TomahawkDesktopWindow::showOfflineSources()
{
    m_sourcetree->showOfflineSources( ui->actionShowOfflineSources->isChecked() );
    TomahawkSettings::instance()->setShowOfflineSources( ui->actionShowOfflineSources->isChecked() );
}


void
TomahawkDesktopWindow::loadSpiff()
{
    bool ok;
    QString urlstr = QInputDialog::getText( this, tr( "Load XSPF" ), tr( "Path:" ), QLineEdit::Normal, "http://ws.audioscrobbler.com/1.0/tag/metal/toptracks.xspf", &ok );
    if ( !ok || urlstr.isEmpty() )
        return;

    XSPFLoader* loader = new XSPFLoader;
    loader->load( QUrl::fromUserInput( urlstr ) );
}


void
TomahawkDesktopWindow::createAutomaticPlaylist( QString playlistName )
{
    QString name = playlistName;

    if ( name.isEmpty() )
        return;

    source_ptr author = SourceList::instance()->getLocal();
    QString id = uuid();
    QString info  = ""; // FIXME
    QString creator = "someone"; // FIXME
    dynplaylist_ptr playlist = DynamicPlaylist::create( author, id, name, info, creator, Static, false );
    playlist->setMode( Static );
    playlist->createNewRevision( uuid(), playlist->currentrevision(), playlist->type(), playlist->generator()->controls(), playlist->entries() );
    ViewManager::instance()->show( playlist );
}


void
TomahawkDesktopWindow::createStation()
{
    bool ok;
    QString name = QInputDialog::getText( this, tr( "Create New Station" ), tr( "Name:" ), QLineEdit::Normal, tr( "New Station" ), &ok );
    if ( !ok || name.isEmpty() )
        return;

    source_ptr author = SourceList::instance()->getLocal();
    QString id = uuid();
    QString info  = ""; // FIXME
    QString creator = "someone"; // FIXME
    dynplaylist_ptr playlist = DynamicPlaylist::create( author, id, name, info, creator, OnDemand, false );
    playlist->setMode( OnDemand );
    playlist->createNewRevision( uuid(), playlist->currentrevision(), playlist->type(), playlist->generator()->controls() );
    ViewManager::instance()->show( playlist );
}


void
TomahawkDesktopWindow::createPlaylist()
{
    PlaylistTypeSelectorDlg* playlistSelectorDlg = new PlaylistTypeSelectorDlg( TomahawkApp::instance()->mainWindow(), Qt::Sheet );
#ifndef Q_OS_MAC
    playlistSelectorDlg->setModal( true );
#endif
    connect( playlistSelectorDlg, SIGNAL( finished( int ) ), this, SLOT( playlistCreateDialogFinished( int ) ) );

    playlistSelectorDlg->show();
}

void TomahawkDesktopWindow::playlistCreateDialogFinished( int ret )
{
    PlaylistTypeSelectorDlg* playlistSelectorDlg = qobject_cast< PlaylistTypeSelectorDlg* >( sender() );
    Q_ASSERT( playlistSelectorDlg );

    QString playlistName = playlistSelectorDlg->playlistName();
    if ( playlistName.isEmpty() )
        playlistName = tr( "New Playlist" );

    if ( !playlistSelectorDlg->playlistTypeIsAuto() && ret ) {

        playlist_ptr playlist = Tomahawk::Playlist::create( SourceList::instance()->getLocal(), uuid(), playlistName, "", "", false, QList< query_ptr>() );
        ViewManager::instance()->show( playlist );
    } else if ( playlistSelectorDlg->playlistTypeIsAuto() && ret ) {
       // create Auto Playlist
       createAutomaticPlaylist( playlistName );
    }
    playlistSelectorDlg->deleteLater();
}

void
TomahawkDesktopWindow::audioStarted()
{
    ui->actionPlay->setText( tr( "Pause" ) );
}


void
TomahawkDesktopWindow::audioStopped()
{

    ui->actionPlay->setText( tr( "Play" ) );
}


void
TomahawkDesktopWindow::onPlaybackLoading( const Tomahawk::result_ptr& result )
{
    m_currentTrack = result;
    setWindowTitle( m_windowTitle );
}

void
TomahawkDesktopWindow::onSipConnected()
{
    ui->actionToggleConnect->setText( tr( "Go &offline" ) );
}


void
TomahawkDesktopWindow::onSipDisconnected()
{
    ui->actionToggleConnect->setText( tr( "Go &online" ) );
}


void
TomahawkDesktopWindow::onSipPluginAdded( SipPlugin* p )
{
    connect( p, SIGNAL( addMenu( QMenu* ) ), this, SLOT( pluginMenuAdded( QMenu* ) ) );
    connect( p, SIGNAL( removeMenu( QMenu* ) ), this, SLOT( pluginMenuRemoved( QMenu* ) ) );
}


void
TomahawkDesktopWindow::onSipPluginRemoved( SipPlugin* p )
{
    Q_UNUSED( p );
}


void
TomahawkDesktopWindow::onSipError()
{
    onSipDisconnected();

    QMessageBox::warning( this,
                          tr( "Authentication Error" ),
                          QString( "Error connecting to SIP: Authentication failed!" ),
                          QMessageBox::Ok );
}


void
TomahawkDesktopWindow::showAboutTomahawk()
{
    QMessageBox::about( this, tr( "About Tomahawk" ),
                        tr( "<h2><b>Tomahawk %1<br/>(%2)</h2>Copyright 2010, 2011<br/>Christian Muehlhaeuser &lt;muesli@tomahawk-player.org&gt;<br/><br/>"
                            "Thanks to: Leo Franchi, Jeff Mitchell, Dominik Schmidt, Jason Herskowitz, Alejandro Wainzinger, Michael Zanetti, Harald Sitter and Steve Robertson" )
                        .arg( TomahawkUtils::appFriendlyVersion() )
                        .arg( qApp->applicationVersion() ) );
}


void
TomahawkDesktopWindow::checkForUpdates()
{
#ifdef Q_WS_MAC
    Tomahawk::checkForUpdates();
#endif
}


void
TomahawkDesktopWindow::onSearch( const QString& search )
{
    if ( !search.trimmed().isEmpty() )
        ViewManager::instance()->show( new SearchWidget( search, this ) );
}


void
TomahawkDesktopWindow::onFilterEdited()
{
    onSearch( m_searchWidget->text() );
    m_searchWidget->clear();
}

void
TomahawkDesktopWindow::showQueue()
{
    if ( QThread::currentThread() != thread() )
    {
        qDebug() << "Reinvoking in correct thread:" << Q_FUNC_INFO;
        QMetaObject::invokeMethod( this, "showQueue", Qt::QueuedConnection );
        return;
    }

    m_queueView->show();
}


void
TomahawkDesktopWindow::hideQueue()
{
    if ( QThread::currentThread() != thread() )
    {
        qDebug() << "Reinvoking in correct thread:" << Q_FUNC_INFO;
        QMetaObject::invokeMethod( this, "hideQueue", Qt::QueuedConnection );
        return;
    }

    m_queueView->hide();
}