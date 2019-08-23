/*
 * qps.cpp
 * This file is part of qps -- Qt-based visual process status monitor
 *
 * Copyright 1997-1999 Mattias Engdegård
 * Copyright 2005-2012 fasthyun@magicn.com
 * Copyright 2015-     daehyun.yang@gmail.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA.
 */

// TODO & MEMO
/*
 *
 * For easy maintance
 *   1. F_CMDLINE should be rightmost field
 *   2. F_PROCESSNAME shoud be leftmost field  and  NEVER removed!
 *
 */

// . Memory Hole or Qt's string allocate bug?
// . clipboard copy [CMDLINE,PID,USERNAME...]
// . [klog] table cache.
// . save sort of tree, linear
// . watchdog showmsg substitude
// . history system
// . cmd [w] analsys: [IDLE] [WHAT]
// . ExecWindow - [running] finish.
// . UNIX Domain SOCKET
// . COLOR : orange FF5d00
// . P_MEM  -> P_%MEM or P_PMEM

#include "../icon/icon.xpm"

#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/utsname.h> // uname
#include <signal.h>
#include <errno.h>
#include <sched.h>
#include <unistd.h> //for sleep

#include "qps.h"
#include "qpsapp.h"
#include "dialogs.h"
#include "commanddialog.h"
#include "commandutils.h"
#include "watchcond.h"
#include "watchdogdialog.h"
#include "lookup.h"
#include "misc.h"

#ifndef USING_PCH
#include <QBitmap>
#include <QTimerEvent>
#include <QVBoxLayout>
#include <QShortcut>
#include <QMenuBar>
#include <QFont>
#include <QFontComboBox>
#include <QToolTip>
#include <QTabWidget>
#include <QApplication>
#include <QCheckBox>
#include <QToolButton>
#include <QMessageBox>
#include <QSessionManager>
#include <QTextEdit>
#include <QSystemTrayIcon>
#include <QSplitter>
#endif

#include <QDockWidget>
#include <QSplitter>
/* --------------------- Global Variable START ---------------------- */
QList<Command *> commands;

bool previous_flag_show_thread_prev = false; // previous state
bool flag_show_thread = false; // to see informations at the thread level
int flag_thread_ok = true;     // we presume a kernel 2.6.x using NPTL
bool flag_session_start = false;
bool flag_start_mini = false; // Need for Xorg.Session
bool flag_refresh = true;     // DEL
bool flag_xcompmgr = false;   // DEL test.compiz..
bool flag_devel = false;
bool flag_schedstat = false;
bool flag_smallscreen = false;
bool flag_firstRun = true; // test

Qps *qps;
// ControlBar 		*controlbar=NULL;
SearchBox *search_box = NULL;
TFrame *infobox = NULL; // testing
// DEL Screenshot 		*screenshot=NULL;
QFontComboBox *font_cb = NULL;
WatchdogDialog *watchdogDialog = NULL;

/// PDisplay *pdisplay;

QList<Procview *> proclist;

#include "trayicon.h"
TrayIcon *trayicon = NULL;

// for Non-ASCII Languages (CJK:chinese,japanese,korean, arabic ...)
#include <QTextCodec>
QTextCodec *codec = NULL;
#define UniString(str) str // codec->toUnicode(str)
/* ------------------------ END global variables -------------------------- */

// default values of settings, overridden by $HOME/.qpsrc if present
bool Qps::flag_show =
    true; // window state of last run : mini(iconic) or normal window
bool Qps::flag_exit = true;
bool Qps::flag_qps_hide = true; // TEST
bool Qps::flag_useTabView = false;
bool Qps::show_file_path = false;
bool Qps::show_cmd_path = true;
bool Qps::show_infobar = true;
bool Qps::show_ctrlbar = true;
bool Qps::show_cpu_bar = true;
bool Qps::show_load_graph = true; // DEL
bool Qps::show_statusbar = true;
bool Qps::load_in_icon = true;
bool Qps::auto_save_options = true;
#ifdef LINUX
bool Qps::hostname_lookup = true;
bool Qps::service_lookup = true;
#endif
bool Qps::pids_to_selection = true;
bool Qps::vertical_cpu_bar = false; // not used
#ifdef SOLARIS
bool Qps::normalize_nice = true;
bool Qps::use_pmap = true;
#endif
bool Qps::tree_gadgets = true;
bool Qps::tree_lines = true;
int Qps::swaplimit = 5; // default: warn when less than 10% swap left
bool Qps::swaplim_percent = true;

QThread *thread_main = 0;

Qps::Qps()
{
    setObjectName("qps_main_window");
    timer_id = 0;
    field_win = 0;
    prefs_win = 0;
    command_win = 0;
    default_icon = 0;
    default_icon_set = false;

    setIconSize(24, 24); // Important!!
                         //	setMouseTracking(true); // no need?

    if (flag_devel)
    {
        thread_main = thread(); // Test
        printf("qps thread =%p , qApp thread =%p\n", thread(),
               QApplication::instance()->thread());
    }

    // watchdogDialog = new WatchdogDialog; //Memory Leak

    //	font_cb=new QFontComboBox(this); // preload
    //	font_cb->setWritingSystem ( QFontDatabase::Latin );
    // font_cb->hide();

    make_signal_popup_menu();

    // MOVETO Pstable !!
    m_headpopup = new QMenu( tr( "header_popup" ), this);
    m_headpopup->addAction( tr( "Remove Field" ), this, SLOT(menu_remove_field()));
    m_fields = new QMenu( tr( "Add Field" ), this);
    m_headpopup->addMenu(m_fields);
    // connect(m_fields, SIGNAL(activated(int)),
    // SLOT(add_fields_menu(int)));
    // m_headpopup->addAction("Select Field", this, SLOT(menu_custom()) );

    m_command = new QMenu( tr( "Command" ), this); // filled in later

    QAction *act;

    m_view = new QMenu( tr( "View" ), this);
    act = m_view->addAction( tr( "Process" ) ); // act->setData(Procview::CUSTOM);
    act = m_view->addAction( tr( "Log" ) );     // act->setData(Procview::CUSTOM);
    // m_view->hide();

    m_field = new QMenu("Field", this);
    act = m_field->addAction( tr( "Custom Fields" ) );
    act->setData(Procview::CUSTOM);
    act = m_field->addAction( tr( "Basic Fields " ) );
    act->setData(Procview::USER);
    act = m_field->addAction( tr( "Jobs Fields " ) );
    act->setData(Procview::JOBS);
    act = m_field->addAction( tr( "Memory Fields " ) );
    act->setData(Procview::MEM);
#ifdef LINUX
    act = m_field->addAction( tr( "Scheduling Fields " ) );
    act->setData(Procview::SCHED);
#endif

    m_field->addSeparator();
    act = m_field->addAction( QIcon::fromTheme(QStringLiteral("edit-find-replace"))
                            , tr( "Select Custom Fields..." )
                            , this
                            , SLOT(menu_custom()));
    act->setData(MENU_CUSTOM);

    connect(m_field, SIGNAL(triggered(QAction *)), this,
            SLOT(view_menu(QAction *)));
    connect(m_field, SIGNAL(aboutToShow()), SLOT(update_menu_status()));

    /// connect(m_view, SIGNAL(triggered(QAction *)),this,
    /// SLOT(view_menu(QAction
    /// *)));
    /// connect(m_view, SIGNAL(aboutToShow ()), SLOT(update_menu_status()));

    m_options = new QMenu( tr( "Option" ), this);
    m_options->addAction( tr( "Update Period..." ), this, SLOT(menu_update()));
    m_options->addSeparator();
    act = m_options->addAction("", /* MENU_PATH */ this,
                               SLOT(menu_toggle_path()));
    act->setData(QVariant(MENU_PATH));
    act = m_options->addAction("", this, SLOT(menu_toggle_infobar()));
    act->setData(QVariant(MENU_INFOBAR));
    act = m_options->addAction("", this, SLOT(menu_toggle_ctrlbar()));
    act->setData(QVariant(MENU_CTRLBAR));
    act = m_options->addAction( tr( "Show Status bar" )
                              , this
                              ,SLOT(menu_toggle_statusbar()));
    act->setData(QVariant(MENU_STATUS));
    act = m_options->addAction("", this, SLOT(menu_toggle_cumul()));
    act->setData(QVariant(MENU_CUMUL));

    m_options->addSeparator();
    m_options->addAction( QIcon::fromTheme(QStringLiteral("preferences-system"))
                        , tr( "Preferences..." )
                        , this
                        , SLOT(menu_prefs())); // MENU_PREFS

    connect(m_options, SIGNAL(aboutToShow()), SLOT(update_menu_status()));

    QMenu *m_help = new QMenu( tr( "Help" ), this);
    // m_help->addAction("FAQ", this, SLOT(license()));
    m_help->addAction(QIcon::fromTheme("help-about"),  tr( "About" ), this,
                      SLOT(about()));

    // menu = new QMenuBar(this);
    menubar = new QMenuBar;
    menubar->addMenu(m_command);
    // am_view=menubar->addMenu(m_view);
    menubar->addMenu(m_field);
    menubar->addMenu(m_options);
    menubar->addSeparator();
    menubar->addMenu(m_help);

    ctrlbar = new ControlBar(this);
    // controlbar=ctrlbar;

    connect(ctrlbar, SIGNAL(need_refresh()), SLOT(refresh()));
    connect(ctrlbar, SIGNAL(viewChange(QAction *)),
            SLOT(view_menu(QAction *))); //?????

    context_col = -1;

    procview = new Procview(); // refresh() only not rebuild()
/// procview = new Procview("localhost"); // refresh() only not rebuild()
/// procview->refresh(); // TODO
/// proclist.append(procview);
/// procview->start(); // thread start

#ifdef HTABLE2
    pstable = new Pstable2(this, procview); //
#else
    pstable = new Pstable(this, procview); // no refresh()
#endif
    PDisplay *display = new PDisplay(this);
    infobar = display->addSystem(procview);
    //.	infobar = new Infobar(this,procview); // graph_bar
    //	infobar = new Infobar(procview); // graph_bar
    statusBar = new StatusBar(this);

    if (!read_settings())
    {
        // printf("default value\n");
        set_update_period(1300); // default
        resize(640, 370);        // default initial size
    }
		pstable->fontChange();
		ctrlbar->reloadState();

    set_table_mode(procview->treeview); //  Pstable::refresh() occur
    make_command_menu();                // after qpsrc reading

    // misc. accelerators
    QShortcut *c1 =
        new QShortcut(Qt::CTRL + Qt::Key_Q, this, SLOT(save_quit()));
    QShortcut *c3 = new QShortcut(Qt::CTRL + Qt::Key_Space,
                                  ctrlbar->pauseButton, SLOT(click()));
    QShortcut *c2 =
        new QShortcut(Qt::CTRL + Qt::Key_L, pstable, SLOT(repaintAll()));

    // MOVETO : pstable better? hmmm...
    // where is leftClick?
    connect(pstable, SIGNAL(doubleClicked(int)), SLOT(open_details(int)));
    connect(pstable, SIGNAL(rightClicked(QPoint)), this,
            SLOT(show_popup_menu(QPoint)));
    connect(pstable->header(), SIGNAL(rightClicked(QPoint, int)), this,
            SLOT(context_heading_menu(QPoint, int)));
    //	connect(netable, SIGNAL(rightClicked(QPoint)), this,
    // SLOT(context_row_menu(QPoint)));

    selection_items_enabled = true; // ????
    update_load_time = 0;
    //// update_menu_status();

    QVBoxLayout *vlayout = new QVBoxLayout;
    vlayout->setMargin(0);
    if (flag_useTabView)
        vlayout->setSpacing(0);
    else
        vlayout->setSpacing(1);

    // vlayout->addWidget(menu);
    vlayout->setMenuBar(menubar);
    vlayout->addWidget(display);
    // vlayout->addWidget( infobar);
    vlayout->addWidget(ctrlbar);

    /// vlayout->addSpacing(5);

    // if(flag_devel){
    if (0)
    {
        if (0)
        {
            QSplitter *splitter = new QSplitter(Qt::Vertical);
            QDockWidget *dock = new QDockWidget(tr("Detail"), this);
            dock->setFeatures(QDockWidget::DockWidgetClosable);
            // dock->setWidget();
            // dock->setAllowedAreas(Qt::LeftDockWidgetArea |
            // Qt::RightDockWidgetArea);
            // customerList = new QListWidget(dock);
            // vlayout->addStretch();
            splitter->addWidget(pstable);
            splitter->addWidget(dock);
            vlayout->addWidget(splitter);
            vlayout->addWidget(statusBar);
            setLayout(vlayout);
        }
    }

    logbox = new QTextEdit(this);
    logbox->setReadOnly(true);

    if (0) // if(flag_smallscreen==false)
    {
        //
    }
    else
    {
        vlayout->addWidget(pstable);
        vlayout->addWidget(logbox);
        logbox->hide();
    }

    vlayout->addWidget(statusBar);
    setLayout(vlayout);

    infobox = new TFrame(this);
    //	setAttribute(Qt::WA_ShowWithoutActivating);
    if (update_period != eternity)
        Qps::update_timer(); // startTimer(update_period);
    ///	setFocusPolicy (Qt::WheelFocus);

    bar_visibility(); // need

    // testing
    popupx = new QMenu( tr( "test" ), this);
    popupx->addAction( tr( "Copied to Clipboard" ) );
}

// explicit destructor needed for gcc
Qps::~Qps() {}
void Qps::tabChanged(int idx)
{
    /*
    for(int i; i<1024;i++)
    {
            QFont font;
            if(font_cb)
                    font=font_cb->currentFont();
            //font=font_cb->currentFont();
            QApplication::setFont(font);
    }

    printf("tab\n");
    */

    if (idx == 0)
    {
        procview->viewproc = Procview::ALL;
        pstable->refresh();
        //	pstable->update();
    }
    else if (idx == 1)
    {
        // procview->viewproc=Procview::NETWORK;
        // netable->refresh();
        // netable->update();
    }
}
// return true if all selected processes are stopped
bool Qps::all_selected_stopped()
{
    for (int i = 0; i < procview->linear_procs.size(); i++)
    {
        Procinfo *p = procview->linear_procs[i];
        if (p->selected && p->state != 'T')
            return false;
    }
    return true;
}

// Adjust menu to contain Stop or Continue
void Qps::adjust_popup_menu(QMenu *m, bool cont)
{
    // int idx = m->indexOf(MENU_DETAILS);
    if (procview->treeview == true)
    {
        //	m->setItemVisible (MENU_PARENT,false);
        //	m->setItemVisible (MENU_CHILD,false);
        //	m->setItemVisible (MENU_DYNASTY,false);
    }
    else
    {
        //	m->setItemVisible (MENU_PARENT,true);
        //	m->setItemVisible (MENU_CHILD,true);
        //	m->setItemVisible (MENU_DYNASTY,true);
    }

    if (procview->viewproc == Procview::HIDDEN)
    {
        //	m->setItemVisible (MENU_ADD_HIDDEN,false);
        //	m->setItemVisible (MENU_REMOVE_HIDDEN,true);
    }
    else
    {
        //	m->setItemVisible (MENU_ADD_HIDDEN,true);
        //	m->setItemVisible (MENU_REMOVE_HIDDEN,false);
    }
}

//
void Qps::adjust_popup_menu()
{
    bool allstop = all_selected_stopped();
    QList<QAction *> list = m_popup->actions();
    // printf("adjust_popup_menu () size=%d \n",list.size());

    if (allstop)
        for (int i = 0; i < list.size() - 1; i++)
        {
            QAction *act = list[i];
            int id = act->data().toInt();
            if (id == MENU_SIGCONT)
                act->setVisible(true);
            if (id == MENU_SIGSTOP)
                act->setVisible(false);
        }
    else
        for (int i = 0; i < list.size() - 1; i++)
        {
            QAction *act = list[i];
            int id = act->data().toInt();
            if (id == MENU_SIGCONT)
                act->setVisible(false);
            if (id == MENU_SIGSTOP)
                act->setVisible(true);
        }
}

// build signal menu (used in two places)
QMenu *Qps::make_signal_popup_menu()
{

    // move to pstable?
    QAction *act; // show_popup_menu() callback
    m_popup = new QMenu( tr( "context popup" ), this);
    m_popup->addAction( tr( "Renice..." ), this, SLOT(menu_renice()));
    m_popup->addAction( tr( "Scheduling..." ), this, SLOT(menu_sched()));
    m_popup->addSeparator();
    m_popup->addAction( tr( "Terminate" ), this, SLOT(sig_term()),
                       Qt::Key_Delete); // better
    m_popup->addAction( tr( "Hangup" ), this, SLOT(sig_hup()), Qt::ALT + Qt::Key_H);
    m_popup->addAction( tr( "Kill" ), this, SLOT(sig_kill()), Qt::ALT + Qt::Key_K);
    act = m_popup->addAction( tr( "Stop" ), this, SLOT(sig_stop()));
    act->setData(MENU_SIGSTOP);
    act = m_popup->addAction( tr( "Continue" ), this, SLOT(sig_cont()));
    act->setData(MENU_SIGCONT);

    //	connect(m_popup, SIGNAL(aboutToShow ()),this,
    // SLOT(adjust_popup_menu()));
    ///	m_popup->addAction("Find Parent", 	this,
    /// SLOT(menu_parent()),0,MENU_PARENT);
    ///	m_popup->addAction("Find Children", 	this,
    /// SLOT(menu_children()),0,MENU_CHILD);
    ///	m_popup->addAction("Find Descendants", this,
    /// SLOT(menu_dynasty()),0,MENU_DYNASTY);

    QMenu *m = new QMenu("Other Signals");
    act = m->addAction( tr( "SIGINT (interrupt)" ) );
    act->setData(SIGINT);
    act = m->addAction( tr( "SIGCONT (continue)" ) );
    act->setData(SIGCONT);
    act = m->addAction( tr( "SIGSTOP (stop)" ) );
    act->setData(SIGSTOP);
    act = m->addAction( tr( "SIGQUIT (quit)" ) );
    act->setData(SIGQUIT);
    act = m->addAction( tr( "SIGILL (illegal instruction)" ));
    act->setData(SIGILL);
    act = m->addAction( tr( "SIGABRT (abort)" ) );
    act->setData(SIGABRT);
    act = m->addAction( tr( "SIGFPE (floating point exception)" ) );
    act->setData(SIGFPE);
    act = m->addAction( tr( "SIGSEGV (segmentation violation)" ) );
    act->setData(SIGSEGV);
    act = m->addAction( tr( "SIGPIPE (broken pipe)" ) );
    act->setData(SIGPIPE);
    act = m->addAction( tr( "SIGALRM (timer signal)" ) );
    act->setData(SIGALRM);
    act = m->addAction( tr( "SIGUSR1 (user-defined 1)" ) );
    act->setData(SIGUSR1);
    act = m->addAction( tr( "SIGUSR2 (user-defined 2)" ) );
    act->setData(SIGUSR2);
    act = m->addAction( tr( "SIGCHLD (child death)" ) );
    act->setData(SIGCHLD);
    act = m->addAction( tr( "SIGTSTP (stop from tty)" ) );
    act->setData(SIGTSTP);
    act = m->addAction( tr( "SIGTTIN (tty input)" ) );
    act->setData(SIGTTIN);
    act = m->addAction( tr( "SIGTTOU (tty output)" ) );
    act->setData(SIGTTOU);

    connect(m, SIGNAL(triggered(QAction *)), SLOT(signal_menu(QAction *)));

    m_popup->addMenu(m);
    m_popup->addSeparator();
    m_popup->addAction( tr( "View Details" ), this, SLOT(Action_Detail()));

    return m;
}

void Qps::signal_menu(QAction *act)
{
    int signal = act->data().toInt();
    send_to_selected(signal);
}

#ifdef MOSIX

// build migrate menu
static int intcmp(const int *a, const int *b) { return *a - *b; }

QMenu *Qps::make_migrate_menu()
{
    QString buf;
    QMenu *m = new QMenu;
    Svec<int> lst = Procinfo::mosix_nodes();
    lst.sort(intcmp); /// sort
    m->insertItem("Home", 1);
    m->insertItem("Find Best", 0);
    for (int i = 0; i < lst.size(); i++)
    {
        buf.sprintf("to node %d", lst[i]);
        m->insertItem(buf, lst[i] + 1);
    }
    return m;
}
#endif // MOSIX

// update the visibility of the {info, control, status} bar
void Qps::bar_visibility()
{

    if (show_infobar)
        infobar->show();
    else
        infobar->hide();

    if (show_ctrlbar)
        ctrlbar->show();
    else
        ctrlbar->hide();

    if (show_statusbar)
        statusBar->show();
    else
        statusBar->hide();

    if (flag_useTabView)
    {
        // DEL tbar->showTab(true);
        /// am_view->setVisible(false);
    }
    else
    {
        // DEL tbar->showTab(false);
        // am_view->setVisible(true);
    }
}

void Qps::timerEvent(QTimerEvent *e) { Qps::refresh(); }
//
// dialogs.cpp:    qps->update_timer();
void Qps::update_timer()
{
    if (timer_id)
        killTimer(timer_id);
    timer_id = startTimer(update_period); // *** important !!!
}

// change the update period, recomputing the averaging factor
void Qps::set_update_period(int milliseconds)
{
    if (milliseconds == 0)
    {
        qDebug("DEBUG: set_update_period(): update= %dms \n", milliseconds);
        milliseconds = 1000;
    }
    update_period = milliseconds;
    Proc::update_msec = milliseconds; // testing transfer...
    Procview::avg_factor = exp(-(float)update_period / Procview::cpu_avg_time);
}

void Qps::refresh()
{
    if (flag_refresh == false)
        return;

    procview->refresh(); // important
    // infobar->update_load(); // add_point and drawGraphOnPixmap();
    infobar->updatePixmap(); // add_point and drawGraphOnPixmap();

    if (isVisible())
    { // infobar->isVisible()
        // printf("paint\n");
        // if(search_box and !search_box->hasFocus())
        // search_box->setFocus(Qt::OtherFocusReason);
        pstable->refresh();
        // if(netable and netable->isVisible())netable->refresh();
        if (show_infobar)      // if(Qps::show_load_graph)
            infobar->update(); // TODO: thread update or
                               // infobar->repaint();
                               //==> pdisplay->update();
        // DEL update_menu_selection_status(); // update pop menu
    }

    if (trayicon)
        update_icon(); // make icon for systray
    refresh_details();
}

// make next timer_refresh happen a little earlier to remove processes that
// might have died after a signal
void Qps::earlier_refresh()
{
    const int delay = 500; // wait no more than this period (ms)
    if (update_period > delay && update_period != eternity)
    {
    }
}

void Qps::resizeEvent(QResizeEvent *e)
{
    // DEBUG("Qps::resize() w=%d\n",e->size().width());
    // QWidget::resizeEvent(event);

    int w;
    return;

    // search_box->isVisible(true); // imidiate
    w = ctrlbar->sizeHint().width();
    if (!search_box->isVisible())
        w += search_box->sizeHint().width();

    if (e->size().width() < w)
    {
        // ctrlbar->
        search_box->hide();
    }
    //	else 	search_box->show();

    return;
    if (e->size().width() < 639)
    {
        //	tbar->setDrawBase (false);
        flag_smallscreen = true;
        statusBar->hide();
        //	void QTabWidget::setTabEnabled ( int index, bool enable
        //)
    }
    else
    {
        flag_smallscreen = false;
        statusBar->show();
    }
}

/*
 Description :
        1. called when visible() or hide() state.
        2. called when clicked  WINDOW_EXIT_X_BUTTON
        3. NOT called when user logout (session manager)
*/
void Qps::closeEvent(QCloseEvent *e)
{
    // DEBUG("DEBUG: closeEvent()....\n");
    // sleep(2);
    if ((Qps::flag_exit == false) and trayicon->hasSysTray())
    {
        e->ignore(); // dont close window!
        hide();
        return;
    }
    e->accept(); // ok. I will be exit Now !!
    save_quit();
}

// call by void Qps::make_command_menu()
//			void signal_handler(int sig)
void Qps::save_quit() // will be removed !
{
    // printf("DEBUG: save_quit()....\n");
    close(); // if another window exists, then no exit. // occurs
             // QCoseEvent!
    save_settings();
    qApp->quit(); // MACRO
}

// NEW Version !
// : write geometry, visible fields and other settings to $HOME/.qpsrc
void Qps::save_settings()
{
    if (Qps::auto_save_options)
        write_settings();
}

void Qps::leaveEvent(QEvent *event)
{
    // printf("out!xx\n");	//works well
}

void Qps::enterEvent(QEvent *event)
{
    // printf("in!\n");	 // works well
}

void Qps::showEvent(QShowEvent *event)
{
    //	printf("showEvent()\n");
    //	event->accept();
}
void Qps::mouseMoveEvent(QMouseEvent *event)
{
    //	printf("Qps::mouseMoveEvent\n"); //work in child surface without
    // setMouseTracking(true);
}

// setFocusPolicy() should on
void Qps::focusInEvent(QFocusEvent *event)
{
    //	printf("focusIn\n"); // not work in compiz,Metacity
    //	infobar->showup();
    //	event->accept();
}

void Qps::focusOutEvent(QFocusEvent *event)
{
    //	printf("focusOut\n"); // not work in compiz,Metacity
    //	event->accept();
}

void Qps::keyPressEvent(QKeyEvent *event)
{
    //	printf("Qps::key\n"); // not work in compiz
    if (search_box)
    {
        if (!search_box->hasFocus())
            search_box->setFocus(Qt::OtherFocusReason);
        search_box->keyPressEvent(event);
    }
    // search_box->setFocusProxy(pstable);
}
void Qps::moveEvent(QMoveEvent *event)
{
    ////infobar->refresh();
    // printf("move\n");
}
void Qps::paintEvent(QPaintEvent *event)
{
    //	printf("paintEvent : Qps\n");
    //	update_icon();
}
void Qps::update_icon()
{
    if (load_in_icon)
        set_load_icon(); // always true
    else
        set_default_icon();
}
// called by update_icon()
void Qps::set_default_icon()
{
    if (!default_icon_set)
    {
        if (!default_icon)
            default_icon = new QPixmap((const char **)icon_xpm);
        setWindowIcon(*default_icon);
        default_icon_set = true;
    }
}

void Qps::set_load_icon()
{
    QPixmap *pm = infobar->load_icon(icon_width, icon_height);
    if (!pm->mask())
    {
        // avoid a fvwm/Qt 1.30 problem and create a filled mask for the
        // icon
        // (without mask, Qt would attempt to use a heuristically
        // created mask)
        QBitmap bm(pm->size());
        bm.fill(Qt::color1);
        pm->setMask(bm);
    }
    // QApplication::setWindowIcon(*pm);
    if (trayicon->hasSysTray())
    {
        trayicon->setIcon(*pm);
        // str.sprintf("Qps %2.2f%",Procinfo::loadQps);
        // trayicon->setToolTip(str);
        // trayicon->showMessage("str",str);
    }
    // else
    setWindowIcon(*pm);
    // QApplication::setWindowIcon(*pm);
    default_icon_set = false;
}

QPixmap *Qps::get_load_icon()
{
    return infobar->load_icon(icon_width, icon_height);
}

//
void Qps::refresh_details()
{
////	details.first();
////Details d;
#ifdef LINUX
// Proc::invalidate_sockets();
#endif

    for (int i = 0; i < details.size(); ++i)
    {
        /////Details *d=details[i]
        ////if(details[i].isVisible())
        /////details[i].refresh();
    }
    /*
    while((d = details.current()) != 0) {
            if(d->isVisible())
                    d->refresh();
            details.next();
    } */
}

// called by
//	1.void Qps::field_removed(int index)
//  ====> QMenu::aboutToShow ()
void Qps::update_menu_status()
{
    //	printf("update_menu_status\n");
    update_menu_selection_status();
    // ctrlbar->view->setCurrentItem (procview->viewproc); //???

    // Field Menu
    QList<QAction *> list = m_field->actions();
    for (int i = 0; i < list.size() - 1; i++)
    {
        QAction *act = list[i];
        act->setCheckable(true);
        int id = act->data().toInt();
        if (id == procview->viewfields)
            act->setChecked(true);
        else
            act->setChecked(false);
    }

    // View Menu temporary
    /*
    QString str=tbar->tabText(tbar->currentIndex());
    list=m_view->actions();
    for(int i =0 ;i< list.size() ;i++)
    {
            QAction *act=list[i];
            act->setCheckable(true);

            if(act->text()==str)
            {
                    act->setChecked(true);
            }
            else
                    act->setChecked(false);
    } */

    // Option Menu
    list = m_options->actions();
    for (int i = 0; i < list.size() - 1; i++)
    {
        QAction *act = list[i];
        int id = act->data().toInt();

        if (id == MENU_PATH)
        {
            act->setCheckable(true);
            act->setChecked(Procview::flag_show_file_path);
            act->setText("Show File Path");
            //	act->setText(Procview::flag_show_file_path?
            //"Hide File Path" :
            //"Show File Path");
        }
        else if (id == MENU_INFOBAR)
        {
            act->setCheckable(true);
            act->setChecked(show_infobar);
            act->setText("Show Graph");
            // act->setText(show_infobar 	? "Hide Graph" : "Show
            // Graph");
        }
        else if (id == MENU_CTRLBAR)
        {
            act->setCheckable(true);
            act->setChecked(show_ctrlbar);
            act->setText("Show Control Bar");
            // act->setText(show_ctrlbar	? "Hide Control bar" :
            // "Show Control
            // Bar");
        }
        else if (id == MENU_STATUS)
        {
            act->setCheckable(true);
            act->setChecked(show_statusbar);
        }
        else if (id == MENU_CUMUL)
        {
            act->setCheckable(true);
            act->setChecked(Procview::flag_cumulative);
            act->setText("Include Child Times");
            // act->setText(Procview::flag_cumulative 	?
            // "Exclude Child Times" :
            // "Include Child Times");
        }
    }
}

// need to change name , redesign
void Qps::view_menu(QAction *act)
{
    int id = act->data().toInt();
    if (act->text() == "Process")
    {
        // tbar->setCurrentIndex(0);
        return;
    }
    if (act->text() == "Log")
    {
        // tbar->setCurrentIndex(1);
        return;
    }

    int state = id;
    if (id >= Procview::ALL && id <= Procview::HIDDEN)
    {
        if (procview->viewproc != state)
        {
            procview->viewproc = state;
        }
    }

    if (id >= Procview::USER && id <= Procview::CUSTOM)
    {
        if (procview->viewfields != state)
        {
            procview->viewfields = state;
            procview->set_fields();
        }
    }
    pstable->refresh(); // layout
                        ///	update_menu_status();
}

// called when selection changed & update time
// void Qps::update_menu_status()
// void Qps::make_command_menu()
//  DEL.
//  -> show_popup_menu()
void Qps::update_menu_selection_status()
{

    //	bool enabled = (pstable->numSelected() > 0);
    //	if(enabled)
    if (0)
    {
        bool cont = all_selected_stopped();
        adjust_popup_menu(m_popup, cont);
    }

    for (int i = 0; i < commands.size(); i++)
    {
        /*	if (commands[i]->IsNeedProc()==false)
			m_command->setItemEnabled(MENU_FIRST_COMMAND + i, true);
		else	m_command->setItemEnabled(MENU_FIRST_COMMAND + i, enabled);
	*/}
}

// call by SearchBox::keyPressEvent

// slot
void Qps::sig_term() { send_to_selected(SIGTERM); }

void Qps::sig_hup() { send_to_selected(SIGHUP); }

// need
void Qps::sig_stop()
{
    send_to_selected(SIGSTOP);

    // test
    for (int i = 0; i < procview->linear_procs.size(); i++)
    {
        Procinfo *p = procview->linear_procs[i];
        if (p->selected)
        {
            p->test_stop = 1; // who ??
                              // sendsig(p, sig);
        }
    }
}

void Qps::sig_cont()
{
    send_to_selected(SIGCONT);

    // test
    for (int i = 0; i < procview->linear_procs.size(); i++)
    {
        Procinfo *p = procview->linear_procs[i];
        if (p->selected)
            p->test_stop = 0;
    }
}

void Qps::sig_kill() { send_to_selected(SIGKILL); }

void Qps::menu_custom()
{
    ////	view_menu(Procview::CUSTOM); // should !!
    if (field_win)
    {
        field_win->show();
        field_win->raise();
    }
    else
    {
        field_win = new FieldSelect(procview);
        setWindowGroup(field_win);
        field_win->show();
        connect(field_win, SIGNAL(added_field(int)), this,
                SLOT(field_added(int)));
        connect(field_win, SIGNAL(removed_field(int)), this,
                SLOT(field_removed(int)));
    }
}

// MOVE TO PSTABlE ? hmmm wait...
// Interface for  reading_setting()
// SLOT:
//
// called by
// 	1.click Tree_checkbox
//	2.void Qps::view_menu(int id)
void Qps::set_table_mode(bool treemode)
{
    //	qDebug("set_table_mode()\n");
    ctrlbar->setMode(treemode);     // toggle checkbox
    pstable->setTreeMode(treemode); // first  pstable->refresh();
}

// Slot:
// SEGFAULT CODE:  MOVETO Pstable
// called by
//      1. void Qps::add_fields_menu(int id)
//      2. field_win
void Qps::field_added(int field_id)
{
    int where = -1;
    where = pstable->clickedColumn();
    procview->addField(field_id, where);
    pstable->refresh(); // pstable->update(); // repaint
                        // update_menu_status();
}

// MOVETO Proview
// what this hell? SEGFAULT!
// call by
//	1. FieldSelect
//	2. void Qps::menu_remove_field()
void Qps::field_removed(int index)
{
    /// printf("field_removed() index=%d\n",index);
    procview->removeField(index);
    if (procview->treeview and index == F_CMD)
        set_table_mode(false);
    // should be changed to linear mode !!

    ///	update_menu_status(); // ???

    //	pstable->update();	// no
    pstable->refresh();
    context_col = -1; // *** important **** : right clicked column removed
    return;
}

// moveto command?
void Qps::menu_edit_cmd()
{
    if (command_win)
    {
        command_win->show();
        command_win->raise();
    }
    else
    {
        command_win = new CommandDialog();
        setWindowGroup(command_win);
        command_win->show();
        connect(command_win, SIGNAL(command_change()),
                SLOT(make_command_menu()));
    }
}

// init command menu
// callback by CommandDialog::add_new()
void Qps::make_command_menu()
{
    // should clear SIGNAL!!!
    QAction *act;
    m_command->clear();
    m_command->disconnect();

    if (flag_devel)
    {
        m_command->addAction( tr( "WatchDog" ), watchdogDialog,
                             SLOT(show())); //, m_event);
        //	m_command->addAction("ScreenShot", screenshot,
        // SLOT(show()) );
        act = m_command->addAction( tr( "Edit Commands..." ), this,
                                   SLOT(menu_edit_cmd()));
        //	act->setEnabled(false);
    }

    //	if(commands.size())	m_command->addSeparator();
    add_default_command();

    for (int i = 0; i < commands.size(); i++)
    {
        // commands[i]->menu = m_command->insertItem(commands[i]->name,
        // MENU_FIRST_COMMAND + i);
        // commands[i]->menu =
        act = m_command->addAction(commands[i]->name);
        // connect(act, SIGNAL(triggered(bool )),
        // SLOT(signal_menu(bool)));
    }
    connect(m_command, SIGNAL(triggered(QAction *)),
            SLOT(run_command(QAction *)));

    update_menu_selection_status(); // DEL
    //#ifdef SOLARIS
    /* Solaris CDE don't have a tray, so we need a method to terminate */
    m_command->addSeparator();
    m_command->addAction(QIcon::fromTheme(QStringLiteral("application-exit"))
                        , tr( "Quit" )
                        , this
                        , SLOT(save_quit())
                        , Qt::ALT + Qt::Key_Q);
    //#endif
}

// run by MENU_ID ? qt slot?
// void Qps::run_command(int command_id)
void Qps::run_command(QAction *act)
{
    int command_id;
    int i, j, idx = -1;
    // FUNC_START

    // printf("%s\n",qPrintable(act->text()));
    AddLog(act->text());

    // find_command
    for (i = 0; i < commands.size(); i++)
    {
        //	if(commands[i]->menu==command_id)
        if (commands[i]->name == act->text())
        {
            if (commands[idx]->IsNeedProc() == false)
            {
                commands[idx]->call(NULL);
                return;
            }

            for (int i = 0; i < procview->linear_procs.size(); i++)
            {
                Procinfo *p = procview->linear_procs[i];
                if (p->selected)
                    commands[idx]->call(p);
            }
            break;
        }
    }

    return;
}

// detail
void Qps::Action_Detail()
{
    for (int i = 0; i < procview->linear_procs.size(); i++)
    {
        Procinfo *p = procview->linear_procs[i];
        if (p->selected)
            open_details(i);
    }
}

// MOVETO : pstable.cpp
void Qps::open_details(int row)
{
    Procinfo *p = procview->linear_procs[row];
    if (p->detail)
    {
        //		p->detail->raise();
        p->detail->show();
        printf("show detail \n");
    }
    else
    {
        Details *d = new Details(p, procview);
        //	details.append(*d);
        //	setWindowGroup(d);
        d->show();
        ///	connect(d, SIGNAL(closed(Details *)), this,
        /// SLOT(details_closed(Details
        ///*)));
    }
}

void Qps::details_closed(Details *d)
{
    // printf("details_closed()\n");
    // disconnect

    // This is potentially dangerous, since this is called in response to a
    // signal sent by the widget that is about to be deleted here. Better
    // hope
    // that nobody references the object down the call chain!
    // what ??????

    //////details.removeAt(*d);	// deletes window
    // details.removeAt(*d);	// deletes window
}

// find parents of selected processes
void Qps::menu_parent() { locate_relatives(&Procinfo::ppid, &Procinfo::pid); }

void Qps::menu_children() { locate_relatives(&Procinfo::pid, &Procinfo::ppid); }

// Find processes whose attribute b is equal to the attribute a of
// selected processes. Center around topmost found.
// This is quadratic in worst case (shouldn't be a problem)
// called by
// 		1.menu_children()
// 		2.menu_parent()
void Qps::locate_relatives(int Procinfo::*a, int Procinfo::*b)
{
    QList<int> relatives;
    const int infinity = 2000000000;
    int topmost = infinity;
    for (int i = 0; i < procview->linear_procs.size(); i++)
    {
        Procinfo *p = procview->linear_procs[i];
        if (p->selected)
        {
            pstable->setSelected(i, false);
            for (int j = 0; j < procview->linear_procs.size(); j++)
            {
                Procinfo *q = procview->linear_procs[j];
                if (p->*a == q->*b)
                {
                    relatives.append(j);
                    if (j < topmost)
                        topmost = j;
                }
            }
        }
    }
    for (int i = 0; i < relatives.size(); i++)
        pstable->setSelected(relatives[i], true);
    /// if(topmost < infinity) pstable->centerVertically(topmost);
    ////	pstable->selectionNotify();
}

// select all (direct and indirect) offsprings of currently selected
// processes, without deselecting them
void Qps::menu_dynasty()
{
    QList<int> family;
    for (int i = 0; i < procview->linear_procs.size(); i++)
        if (pstable->isSelected(i))
            family.append(i);
    for (int i = 0, j = family.size(); i < j;)
    {
        for (int k = 0; k < procview->linear_procs.size(); k++)
        {
            Procinfo *p = procview->linear_procs[k];
            for (int m = i; m < j; m++)
            {
                Procinfo *q = procview->linear_procs[family[m]];
                if (q->pid == p->ppid)
                    family.append(k);
            }
        }
        i = j;
        j = family.size();
    }
    const int infinity = 2000000000;
    int topmost = infinity;
    for (int i = 0; i < family.size(); i++)
    {
        pstable->setSelected(family[i], true);
        if (family[i] < topmost)
            topmost = family[i];
    }
    ////if(topmost < infinity) pstable->centerVertically(topmost);
    ////pstable->selectionNotify();
}

// CALLBACK: called when right button is clicked in table
void Qps::show_popup_menu(QPoint p)
{
#ifdef MOSIX
    bool may_migrate = false;
    for (int i = 0; i < procview->linear_procs.size(); i++)
    {
        Procinfo *p = procview->linear_procs[i];
        if (p->selected && p->cantmove.isEmpty())
        {
            may_migrate = true;
            break;
        }
    }
    m_popup->setItemEnabled(POPUP_MIGRATE, may_migrate);
#endif
    adjust_popup_menu();
    m_popup->popup(p);
}

// called when right button is clicked in heading
//
void Qps::context_heading_menu(QPoint p, int col)
{
    context_col = col; // ****
    ////printf("context_col=%d\n",col);
    static int init = 0;
    // rebuild the submenu: only include non-displayed fields
    int ncats = procview->categories.size();

    if (init == 0)
    {
        /*	#ifdef SHash
		SHash<int,Category *>::const_iterator it=procview->categories.begin();
		QList<int> keys;
		for ( ; it != procview->categories.end(); it++ )
    		keys.append((*it).first);
		#else
	*/ QList<int> keys = procview->categories.keys();
        //	#endif
        for (int i = 0; i < ncats; i++)
        {
            Category *cat = procview->categories[keys.takeFirst()];
            QAction *act = m_fields->addAction(cat->name);
            act->setData(cat->id); // index==key
        }
        connect(m_fields, SIGNAL(triggered(QAction *)), this,
                SLOT(add_fields_menu(QAction *)));
        init = 1;
    }

    QBitArray displayed(64); // MAX_FIELDS
    displayed.fill(false);
    // printf("categories.size=%d cats.size=%d\n",ncats,
    // procview->cats.size());

    for (int i = 0; i < procview->cats.size(); i++)
    {
        //	printf("cat id=%d \n",procview->cats[i]->id);
        displayed.setBit(procview->cats[i]->id);
    }

    QList<QAction *> la = m_fields->actions();
    for (int j = 0; j < la.size(); j++)
    {
        QAction *act = la[j];
        int id = act->data().toInt();
        if (!displayed.testBit(id))
            act->setVisible(true);
        else
            act->setVisible(false);
    }

    m_headpopup->popup(p);
}

void Qps::add_fields_menu(QAction *act)
{
    // act->text();
    int id = act->data().toInt();
    field_added(id);
    // add_fields_menu(id);
}

// called when field is added from heading context menu
// called by 1. context_heading_menu
void Qps::add_fields_menu(int cat_id)
{
    field_added(cat_id);
    context_col = -1;
    // if(field_win) field_win->update_boxes(); // change to showEvent. !!!!
}

//
void Qps::menu_remove_field()
{
    if (context_col < 0)
        return;
    /// printf("cats.size=%d , context_col=%d
    /// \n",procview->cats.size(),context_col);
    field_removed(procview->cats[context_col]->index);
}

void Qps::menu_update()
{

    QString txt;
    if (update_period == eternity)
        txt = "1 s";
    else if (update_period % 1000 == 0)
        txt.sprintf("%d s", update_period / 1000);
    else
        txt.sprintf("%d ms", update_period);
    IntervalDialog id(txt.toUtf8().data(), update_period != eternity);
    id.exec();
    /// save_settings();
    return;
}

void Qps::menu_toggle_path()
{
    Procview::flag_show_file_path = !Procview::flag_show_file_path;
    /// table refresh() !
}

void Qps::menu_toggle_infobar()
{
    show_infobar = !show_infobar;
    bar_visibility();
}

void Qps::menu_toggle_ctrlbar()
{
    show_ctrlbar = !show_ctrlbar;
    bar_visibility();
}

void Qps::menu_toggle_statusbar()
{
    show_statusbar = !show_statusbar;
    bar_visibility();
}

void Qps::menu_toggle_cumul()
{
    Procview::flag_cumulative = !Procview::flag_cumulative;
    // refresh() !!
}

void Qps::menu_prefs()
{
    if (prefs_win)
    {
        prefs_win->show();
        prefs_win->raise();
    }
    else
    {
        prefs_win = new Preferences();
        setWindowGroup(prefs_win);
        prefs_win->show();
        prefs_win->raise();

        connect(prefs_win, SIGNAL(prefs_change()), this, SLOT(config_change()));
        ///	connect(infobar, SIGNAL(config_change()), prefs_win,
        /// SLOT(update_boxes())); // doesnt need !! --> aboutToShow()
    }
}

// if "Preferences" changed
void Qps::config_change()
{

    write_settings();
    bar_visibility();
		pstable->fontChange();
    // DEL infobar->configure();
    /// resizeEvent(0);		// in case it caused geometry change

    for (int i = 0; i < details.size(); ++i)
    {
        /////	details[i].config_change();
    }
    /* 	while((d = details.current()) != 0) {
                    d->config_change();
                    details.next();
            } */
}

void Qps::menu_renice()
{
    //	if(pstable->hasSelection() == 0)		return;

    int defnice = -1000;

    // use nice of first selected process as default, and check permission
    bool possible = true;
    int euid = geteuid();
    Procinfo *p = 0;

    for (int i = 0; i < procview->linear_procs.size(); i++)
    {
        p = procview->linear_procs[i];
        if (p->selected)
        {
            if (defnice == -1000)
                defnice = p->nice;
            if (euid != 0 && euid != p->uid && euid != p->euid)
                possible = false;
        }
    }

    if (!possible)
    {
        QMessageBox::warning( this
                            , tr( "Permission denied" )
                            , tr( "You do not have permission to renice the selected processes. Only the process owner and the super-user are allowed to do that." ) );
        return;
    }

    int new_nice;
    for (;;)
    {
        SliderDialog sd(defnice, -20, 19); // Linux kernel : -20 ~ 19
        if (!sd.exec())
            return;
        bool ok;
        new_nice = sd.ed_result.toInt(&ok);
        if (ok && new_nice >= -20 && new_nice <= 19)
            break;
    }
    int nicecol = procview->findCol(F_NICE);
    int statcol = procview->findCol(F_STAT);
#ifdef LINUX
    int tmscol = procview->findCol(F_TMS);
#endif

    // do the actual renicing
    for (int i = 0; i < procview->linear_procs.size(); i++)
    {
        Procinfo *p = procview->linear_procs[i];
        if (p->selected)
        {
            if (setpriority(PRIO_PROCESS, p->pid, new_nice) < 0)
            {
                QString s;
                switch (errno)
                {
                case EPERM:
                    // this shouldn't happen, but (e)uid
                    // could be changed...
                    QMessageBox::warning( this
                                        , tr( "Permission denied" )
                                        , tr( "You do not have permission to renice process %1 (%2).Only the process owner and the super-user are allowed to do that." )
                                          .arg( p->pid )
                                          .arg( p->command ) );
                    break;
                case EACCES:
                    QMessageBox::warning( this
                                        , tr( "Permission denied" )
                                        , tr( "Only the super-user may lower the nice value of a process." ) );
                    return;
                }
            }
            else
            {
                p->nice = new_nice; // don't wait for update
#ifdef LINUX
                p->tms = p->get_tms(); // ditto
#endif
            }
        }
    }
}

void Qps::menu_sched()
{
    // if(pstable->hasSelection()==false)	return;
    if (geteuid() != 0)
    {
        QMessageBox::warning( this
                            , tr( "Permission denied" )
                            , tr( "Only the super-user may change the scheduling policy and static priority." ) );
        return;
    }

    // provide reasonable defaults (first selected process)
    Procinfo *p = 0;
    for (int i = 0; i < procview->linear_procs.size(); i++)
    {
        p = procview->linear_procs[i];
        if (p->selected)
            break;
    }
    int pol = p->get_policy();
    int pri = p->get_rtprio();
    SchedDialog sd(pol, pri);
    if (!sd.exec())
        return;

    if (sd.out_policy == SCHED_OTHER)
        sd.out_prio = 0; // only allowed value
    int plcycol = procview->findCol(F_PLCY);
    int rpricol = procview->findCol(F_RPRI);
#ifdef LINUX
    int tmscol = procview->findCol(F_TMS);
#endif
    for (int i = 0; i < procview->linear_procs.size(); i++)
    {
        Procinfo *p = procview->linear_procs[i];
        if (p->selected)
        {
            struct sched_param sp;
            sp.sched_priority = sd.out_prio;
            if (sched_setscheduler(p->pid, sd.out_policy, &sp) < 0)
            {
                if (errno == EPERM)
                {
                    QMessageBox::warning( this
                                        , tr( "Permission denied" )
                                        , tr( "You do not have permission to change the scheduling and/or priority of process %1 (%2). Only the super-user may do that." )
                                          .arg( p->pid )
                                          .arg( p->command ) );
                    break;
                }
            }
            else
            {
                p->policy = sd.out_policy; // don't wait for update
                p->rtprio = sd.out_prio;
#ifdef LINUX
                p->tms = p->get_tms();
#endif
            }
        }
    }
}

#ifdef MOSIX

void Qps::mig_menu(int id) { migrate_selected(id - 1); }

void Qps::migrate_selected(int migto)
{
    // User wants to migrate a process somewhere
    // Write destination into /proc/XX/goto
    int warnremote = 0;
    for (int i = 0; i < procview->linear_procs.size(); i++)
    {
        Procinfo *p = procview->linear_procs[i];
        if (p->selected)
        {
            if (p->isremote)
                ++warnremote;
            char buf[80];
            sprintf(buf, "/proc/%d/goto", p->pid);
            FILE *f = fopen(buf, "w");
            if (f)
            {
                fprintf(f, "%d", migto);
                fclose(f);
            }
        }
    }
    if (warnremote)
        QMessageBox::warning(this, "Remote migration attempt",
                             "You can only migrate an immigrated process "
                             "using qps on the home node.");
    earlier_refresh();
}
#else

// Since this is a slot, at least a stub must be defined even when it isn't
// used (moc ignores preprocessor directives)
void Qps::mig_menu(int) {}

#endif // MOSIX

void Qps::send_to_selected(int sig)
{
    for (int i = 0; i < procview->linear_procs.size(); i++)
    {
        Procinfo *p = procview->linear_procs[i];
        if (p->selected)
            sendsig(p, sig);
    }
    earlier_refresh(); // in case we killed one
}

void Qps::sendsig(Procinfo *p, int sig)
{
    if (kill(p->pid, sig) < 0)
    {
        // if the process is gone, do nothing - no need to alert the
        // user
        if (errno == EPERM)
        {
            QMessageBox::warning( this
                                , tr( "Permission denied" )
                                , tr( "You do not have permission to send a signal to process %1 (%2). Only the super-user and the owner of the process may send signals to it." )
                                  .arg( p->pid )
                                  .arg( p->command ) );
            // PermissionDialog *pd = new
            // PermissionDialog(s,supasswd);
            // pd->exec();
        }
    }
}

// write geometry, visible fields and other settings to $HOME/.qps-settings
#define SETTINGS_FILE ".qpsrc"
// If the file format is changed in any way (including adding new
// viewable fields), QPS_FILE_VERSION must be incremented to prevent
// version mismatches and core dumps

#ifdef LINUX
#define QPS_FILE_VERSION 40 // version of .qps-linux file format
#endif

#ifdef SOLARIS
#define QPS_FILE_VERSION 24 // version of .qps-solaris file format
#endif

struct Sflagvar
{
    const char *name;
    bool *var;
};
static Sflagvar flagvars[] = {{"ExitOnClose", &Qps::flag_exit},
                              {"TabView", &Qps::flag_useTabView},
                              {"SingleCPU", &Procview::flag_pcpu_single},
                              {"devel", &flag_devel},
                              {"cmdpath", &Procview::flag_show_file_path},
                              {"infobar", &Qps::show_infobar},
                              {"ctrlbar", &Qps::show_ctrlbar},
                              {"statbar", &Qps::show_statusbar},
                              {"autosave", &Qps::auto_save_options},
                              {"cpubar", &Qps::show_cpu_bar},
                              {"loadgraph", &Qps::show_load_graph},
                              {"loadicon", &Qps::load_in_icon},
                              {"selectpids", &Qps::pids_to_selection},
                              {"tree", &Procview::treeview},
                              {"cumulative", &Procview::flag_cumulative},
#ifdef LINUX
                              {"hostname", &Qps::hostname_lookup},
                              {"service", &Qps::service_lookup}
#endif
#ifdef SOLARIS
                              {"normalize", &Qps::normalize_nice},
                              {"pmap", &Qps::use_pmap}
#endif
                              ,
                              {0, 0}};

char *settings_path(char *name)
{
    char *home = getenv("HOME");
    if (!home)
        return 0;
    strcpy(name, home);
    strcat(name, "/" SETTINGS_FILE);
    return name;
}

#include <QSettings>
extern QList<watchCond *> watchlist;
// 1.Procview should be contstructed !
// 2.
bool Qps::read_settings()
{
    char path[512];
    if (settings_path(path) == NULL)
    {
        return false;
    }
    QSettings set(path, QSettings::NativeFormat);

    int v = set.value("version", 0).toInt();
    if (v == 0)
        return false;

    int x, y, w, h;
    x = set.value("geometry/x").toInt();
    y = set.value("geometry/y").toInt();
    w = set.value("geometry/width").toInt();
    h = set.value("geometry/height").toInt();
    // set.value("geometry/visiable",isVisible());
    Qps::flag_show = true;
    setGeometry(x, y, w, h);

    if (set.value("font/name") != QVariant() and
        set.value("font/size") != QVariant())
    {
        QString fname = set.value("font/name").toString();
        int fsize = set.value("font/size").toInt(); // if not exist then 0
        QFont font;
        font.setFamily(fname);
        font.setPointSize(fsize);
        QApplication::setFont(font);
    }

    // Field
    procview->cats.clear();
    QStringList sl = set.value("field").toStringList();
    for (int i = 0; i < sl.size(); i++)
    {
        QString str = sl[i];
        procview->addField(str.toUtf8().data());
    }
    /// procview->saveCOMMANDFIELD(); // TMP

    // procview->idxF_CMD=set.value("F_CMD").toInt();

    if (sl.size() ==
        0) // *** for safe , if there is no field. this happen when no qpsrc
    {
        procview->viewfields = Procview::USER;
        procview->set_fields(); ///
    }
    else
    {
        procview->viewfields = Procview::CUSTOM;
    }

    int fid = procview->field_id_by_name( set.value("sort/field").toString() ); // procview->sortcat->name
    int col = procview->findCol(fid);
    if (col >= 0)
        // pstable->setSortColumn(col);  // Pstable::refresh()
        procview->setSortColumn(col); // Pstable::refresh()
    // pstable -> procview
    procview->reversed = set.value("sort/reversed").toBool(); // tmp

    sl = set.value("flags").toStringList();
    // for(int i=0;i<sl.size();i++)
    for (Sflagvar *fs = flagvars; fs->name; fs++)
    {
        if (sl.contains(fs->name))
            *(fs->var) = true;
        else
            *(fs->var) = false;
    }
    // procview->treeview=set.value("treeview").toBool();
    flag_show_thread = set.value("show_thread", false).toBool();

    int i = set.value("interval").toInt();
    set_update_period(i);
    i = set.value("viewproc").toInt();
    procview->viewproc = i;
    ctrlbar->view->setCurrentIndex(
        i); // procview->viewproc=set.value("viewproc").toInt();

    int size = set.beginReadArray("watchdog");
    for (int i = 0; i < size; i++)
    {
        set.setArrayIndex(i);
        watchCond *w = new watchCond;
        w->putstring(set.value("cat").toString());
        watchlist.append(w);
    }
    set.endArray();
    size = set.beginReadArray("commands");
    for (int i = 0; i < size; i++)
    {
        set.setArrayIndex(i);
        Command *c = new Command;
        c->putString(set.value("cmd").toString());
        commands.append(c);
    }
    set.endArray();

    return true;
    /*
            } else if(strcmp(buf, "swaplim") == 0) {
                            swaplimit = atoi(p);
                            swaplim_percent = false;
                            if(swaplimit < 0) {
                                    swaplimit = -swaplimit;
                                    swaplim_percent = true;
                            }
            fclose(f); */
    return true;
}

// USING :
// write geometry, visible fields and other settings to $HOME/.qps-settings
void Qps::write_settings() // save setting
{
    char path[512];
    if (settings_path(path) == NULL)
    {
        return;
    }

    QSettings set(path, QSettings::NativeFormat);

    set.setValue("version", QPS_FILE_VERSION);
    // set.beginGroup("");
    // set.endGroup();
    set.setValue("geometry/x",
                 geometry().x()); // if hide then, wrong value saved!!
    set.setValue("geometry/y", geometry().y());
    set.setValue("geometry/width", geometry().width());
    set.setValue("geometry/height", geometry().height());
    set.setValue("geometry/visiable",
                 isVisible()); // isVisible() ? "show":"hide");
    set.setValue("viewproc",
                 procview->viewproc); //	All, your , running process...
    /// set.setValue("treeview",procview->treeview);

    QStringList sl;
    // printf("procview cats.size=%d\n",procview->cats.size());
    procview->update_customfield();
    set.setValue("field", procview->customfields);
    //	set.setValue("F_CMD",procview->idxF_CMD);

    if (procview->sortcat) // sometimes,  sortcat==NULL.
    {
        set.setValue("sort/field", procview->sortcat->name);
        set.setValue("sort/reversed", procview->reversed);
    }
    //	return;
    sl.clear();
    for (Sflagvar *fs = flagvars; fs->name; fs++)
    {
        if (*(fs->var))
            sl.append(fs->name); // fprintf(f, " %s", fs->name);
    }
    set.setValue("flags", sl);
    set.setValue("show_thread", flag_show_thread);
    
		//	fprintf(f,	"swaplim: %d\n"	"interval: %d\n",
    //				swaplim_percent ? -swaplimit :
    // swaplimit,
    //				update_period);

    sl.clear();
    set.setValue("font/name", QApplication::font().family());
    set.setValue("font/size", QApplication::font().pointSize());

    set.setValue("interval", update_period);

    set.beginWriteArray("watchdog");
    for (int i = 0; i < watchlist.size(); i++)
    {
        set.setArrayIndex(i);
        set.setValue("cat", watchlist[i]->getstring());
    }
    set.endArray();

    set.beginWriteArray("commands");
    for (int i = 0; i < commands.size(); i++)
    {
        set.setArrayIndex(i);
        set.setValue("cmd", commands[i]->getString());
    }
    set.endArray();

    /*
            fprintf(f, "hidden_process:");
            for(int i = 0; i < hidden_process.size(); i++) {
                    if(i!=0) fprintf(f,",");
                    fprintf(f,"%s",(const char *)hidden_process[i]);
            }
    */

    ///	printf("Qps: setting saved !\n");
}

// set the window_group hint to that of the main (qps) window
// DELETE ? -> need function , why?
void Qps::setWindowGroup(QWidget *w)
{
    /*
  XWMHints wmh;

  wmh.flags = WindowGroupHint;
  wmh.window_group = handle();
  XSetWMHints(w->x11Display(), w->handle(), &wmh);
  */
}

// DEL
void Qps::setCommand(int argc, char **argv)
{
    // bug: argv[0] should really be frobbed into an absolute path name here
    /// XSetCommand(x11Display(), handle(), argv, argc);
}

// return host name with domain stripped
QString short_hostname()
{
    //// int gethostname(char *name, size_t len); // hyun?
    struct utsname u;
    uname(&u);
    char *p = strchr(u.nodename, '.');
    if (p)
        *p = '\0';
    QString s(u.nodename);
    return s;
}

bool opt_eq(const char *arg, const char *opt)
{
    if (arg[0] == '-')
        arg++;
    if (arg[0] == '-')
        arg++;

    return strncmp(arg, opt, strlen(opt)) == 0;
}

// print some help to stdout and exit
void print_help(char *cmdname)
{
    fprintf(stderr, "Usage: qps [options]\n"
                    "Options:\n"
                    "  -version\t\tversion\n"
                    "  -mini     \t\tstart Minimized\n");
}

/// #include	<QX11Info>
QByteArray geo;
void Qps::clicked_trayicon()
{
    // QT 4.5 bug : hide(), then show() window's pos will be changed a
    // little.
    if (isMinimized() or (isVisible() == false)) // if hidden
    {
        // printf("showNormal\n");
        // show(); // works..
        showNormal(); // works..
        return;
    }

    // METACITY- focus stealing prevention : XRaiseWindow() not work.
    // Compiz works well !!
    if (isActiveWindow() == false) // if lower than other windows
    {
        raise();
        activateWindow(); // for  WindowMaker
        return;
    }
    hide();
}

void Qps::clicked_trayicon(QSystemTrayIcon::ActivationReason r)
{
    // printf("ActivationReason = %d\n",r);
    if (r == QSystemTrayIcon::Trigger)
    {
        if (!isHidden())
        {
            hide();
        }
        else
        {
            showNormal();
        }
    }
    if (r == QSystemTrayIcon::Context)
    {
    }
}

//
void Qps::hideEvent(QHideEvent *event)
{
    //	printf("hideEvent()\n");
    if (trayicon->hasSysTray())
    {
        //	event->accept();
    }
    //	geo=saveGeometry();
}

#include <signal.h>
void signal_handler(int sig)
{
    //	if(sig==SIGINT)	printf("DEBUG: catched SIGINT \n");
    //	if(sig==SIGTERM)printf("DEBUG: catched SIGTERM \n");
    qps->save_quit();
    // printf("Qps: suiciding.... wait \n");
    printf("Qps: terminating...\n");
}

int main(int argc, char **argv, char **envp)
{
    signal(SIGTERM, signal_handler);
    signal(SIGINT, signal_handler);
    signal(SIGKILL, signal_handler);

    Lookup::initproctitle(argv, envp); // will be REMOVEd

    for (int i = 1; i < argc; i++)
    {
        if (opt_eq(argv[i], "version"))
        {
            fprintf(stderr,
                    "qps version " QPS_VERSION ", using Qt library %s\n",
                    qVersion());
            exit(1);
        }
        else if (opt_eq(argv[i], "help") || opt_eq(argv[i], "h"))
        {
            print_help(argv[0]);
            exit(1);
        }
        else if (opt_eq(argv[i], "session") or opt_eq(argv[i], "sm"))
        {
            flag_session_start = true;
            sleep(2); // **** important !! maybe systray runs later
        }
        else if (opt_eq(argv[i], "min")) // mini
        {
            flag_start_mini = true;
        }
        else if (strstr(argv[i], "screen"))
        {
            // int screenshot_main(int argc, char **argv);
            // screenshot_main(argc,argv);
            // return 1;
        }
    }
    //	codec = QTextCodec::codecForLocale(); // for Local locale
    check_system_requirement(); // check kernel version.. etc in proc.cpp
    check_qps_running();        // check already qps running.  in misc.cpp

    QpsApp app(argc, argv);

    init_misc(0); // init misc, some test code runs ...
    qps = new Qps();
    //	sleep(20);

    QString caption( QString( "%1@%2").arg( getenv( "USER" ) )
                                      .arg( short_hostname() ) ); // geteuid()

    qps->setWindowTitle(UniString(caption));
    qps->setWindowIcon(QPixmap((const char **)icon_xpm));

    // MOVETO  Systray
    QMenu *menu = new QMenu(qps);
    /// menu->addAction( UniString("About"), qps, SLOT(about()) );
    menu->addAction( QObject::tr( "Show" )
                   , qps
                   , SLOT( showNormal() ) );
    menu->addAction( QObject::tr( "Hide" )
                   , qps
                   , SLOT( hide() ) );
    menu->addSeparator();
    if (flag_devel)
    {
        menu->addAction( QObject::tr( "ScreenShot" )
                       , qps
                       , SLOT(start_screenshot()));
    }

    menu->addAction( QObject::tr( "Quit" )
                   , qps
                   , SLOT(save_quit())
                   , Qt::ALT + Qt::Key_Q);

    trayicon = new TrayIcon(QPixmap((const char **)icon_xpm /* init icon */),
                            "qps", menu);
    QObject::connect(trayicon, SIGNAL(clicked(const QPoint &)), qps,
                     SLOT(clicked_trayicon()));
    QObject::connect(trayicon, SIGNAL(doubleClicked(const QPoint &)), qps,
                     SLOT(clicked_trayicon()));
    QObject::connect(trayicon,
                     SIGNAL(activated(QSystemTrayIcon::ActivationReason)), qps,
                     SLOT(clicked_trayicon(QSystemTrayIcon::ActivationReason)));

    trayicon->sysInstall(); // ok

    if (flag_session_start or flag_start_mini)
    {
        if (trayicon->hasSysTray())
        {
            qps->hide(); // qps->setHidden(true);
        }
        else
        {
            qps->showMinimized();
        }
    }
    else
    {
        qps->show();
    }

    // Testing
    //	app.setStyleSheet(" QToolTip { opacity: 100;" "border-width:
    // 1px;
    // border-style: solid;  border-color: rgb(50,110,80); border-radius:
    // 4px ;"
    //"background-color : rgba(0,0,0); padding: 3px; color: rgb(0,255,150);
    //}");
    AddLog( QString( "Qps %1 launched." ).arg( QPS_VERSION ) );

    return app.exec();
}

#include <QDialog>
#include <QTextBrowser>
#include <QDesktopServices>
#include <QDialogButtonBox>
#include <QClipboard>

void Qps::test_popup(const QUrl &link)
{

    QClipboard *clipboard = QApplication::clipboard();
    clipboard->setText(link.toString(), QClipboard::Clipboard);
    popupx->popup(QCursor::pos());
}

void Qps::about()
{
    QDialog *diag = new QDialog(this);
    diag->setWindowTitle( tr( "About" ) );
    QVBoxLayout *lay = new QVBoxLayout(diag);

    QLabel *label = new QLabel(diag);
    //	label->setMinimumSize( 400,200 );
    label->setOpenExternalLinks(true);
    // 	QLabel::setOpenExternalLinks ( bool open );
    QTextBrowser *browser = new QTextBrowser(diag);
    browser->setOpenExternalLinks(false);
    browser->setOpenLinks(false);

    connect(browser, SIGNAL(anchorClicked(const QUrl &)), this,
            SLOT(test_popup(const QUrl &)));

    lay->addWidget(label);
    lay->addWidget(browser);

    //	QDesktopServices::openUrl(QUrl("file:///", QUrl::TolerantMode));
    //

    QString str( tr( "<h2> Qps %1 - A Visual Process Manager </h2> %2 using Qt library %3"
                     "<br><br>"
                     "<b>Source: </b><a href=\"https://github.com/lxqt/qps\">https://github.com/lxqt/qps/</a>"
                     "<br>"
                     "<b>Bugtracker: </b><a href=\"https://github.com/lxqt/qps/issues\">https://github.com/lxqt/qps/issues</a>")
                 .arg( QPS_VERSION )
                 .arg(
#ifdef SOLARIS
#ifdef _LP64
                      "64-bit "
#else
                      "32-bit "
#endif
                      "Solaris version "
#else
                      ""
#endif // SOLARIS
                     )
                 .arg( qVersion() ) );
    label->setText( str );
    str.append( tr( "<b>Original Qps by</b><br>"
                    "Mattias Engdegård (f91-men@nada.kth.se)<br><br>"
                    "<b>Contributors</b><br>"
                    "Olivier.Daudel@u-paris10.fr<br>"
                    "jsanchez@todounix.homeip.net <br>"
                    "daehyun.yang@gmail.com <br>"
                    "Luís Pereira (luis.artur.pereira@gmail.com)<br>"
                    "Alf Gaida (agaida@siduction.org)<br>"
                    "Paulo Lieuthier (paulolieuthier@gmail.com)<br>"
                    "Jerome Leclanche (jerome@leclan.ch)<br>" ) );
    browser->setText(str);

    QDialogButtonBox *bbox = new QDialogButtonBox( QDialogButtonBox::Ok
                                                 , Qt::Horizontal
                                                 , diag );

    lay->addWidget(bbox);

    connect(bbox, SIGNAL(accepted()), diag, SLOT(accept()));

    diag->exec();

    //	mb.setIconPixmap(QPixmap((const char **)icon_xpm));
}

void Qps::license() // -> help()
{

    QDialog *diag = new QDialog(this);
    diag->setWindowTitle("Quick Help");
    // diag->setSizeGripEnabled(true) ;
    QHBoxLayout *lay = new QHBoxLayout(diag);

    QTextBrowser *browser = new QTextBrowser(diag);
    browser->setMinimumSize(400, 200);

    lay->addWidget(browser);

    //	QDesktopServices::openUrl(QUrl("file:///", QUrl::TolerantMode));
    // 	QLabel::setOpenExternalLinks ( bool open );
    browser->setOpenExternalLinks(true);
    browser->setOpenLinks(true);
    browser->setText( tr( "<H1>QPS Help</H1>"
                          "Updated: May 24 2005<BR>"
                          "<A "
                          "HREF=\"http://kldp.net/projects/qps\">http://kldp.net/projects/"
                          "qps</"
                          "A><HR>"

                          "<table style=\"text-align: center; width: 100%;\" border=\"1\""
                          " cellpadding=\"1\" cellspacing=\"0\">"
                          "  <tbody>"
                          "    <tr>"
                          "      <td"
                          " style=\"vertical-align: top; background-color: rgb(204, 204, "
                          "204);\">Quit"
                          "      </td>"
                          "      <td >&nbsp; CTRL + q , CTRL + x"
                          "      </td>"
                          "    </tr>"
                          "    <tr>"
                          "      <td"
                          " style=\"vertical-align: top; background-color: rgb(204, 204, "
                          "204);\">Update"
                          "      </td>"
                          "      <td>&nbsp;Space , Enter "
                          "      </td>"
                          "    </tr>"
                          "    <tr><td> process Terminate </td>  <td> ALT + T , DELETE </td> "
                          "</tr>"
                          "    <tr><td> process Kill </td>  <td> ALT + K  </td> </tr>"
                          "  </tbody>"
                          "</table>" ) );
    diag->exec();
}

// slot
void Qps::start_screenshot() {}

// MOVETO qps::keyPressEvent()
void SearchBox::keyPressEvent(QKeyEvent *e)
{
    if (e->key() == Qt::Key_Delete)
    {
        qps->sig_term();
        return;
    }

    if (e->key() == Qt::Key_Escape)
    {

        clear(); // clear searchbox

        qps->pstable->clearAllSelections(); // TEMP

        // xb->setDown(true);
        // event_xbutton_clicked();
        // xb->setDown(false);
    }
    else
        QLineEdit::keyPressEvent(e);

    qps->procview->filterstr = text();
    qps->pstable->refresh(); /// qps->refresh(); // better ?
}

void STATUSBAR_COUNT_UPDATE() {}

void STATUSBAR_SETCOUNT(int n)
{
    if (qps)
        qps->statusBar->update(n);
}

void PSTABLE_SETTREEMODE(bool mode)
{
    if (qps)
        qps->pstable->setTreeMode(mode);
}

void QPS_SHOW()
{
    if (qps)
        qps->showNormal();
}

// TESTING
int QPS_PROCVIEW_CPU_NUM()
{
    if (qps)
        return qps->procview->num_cpus;
    else
        return 0;
}

void AddLog(QString str)
{
    qDebug() << qps;
    if (qps && qps->logbox)
    {
        qps->logbox->append(str);
    }
}
