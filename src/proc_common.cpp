/*
 * fm-search-uri.c
 * This file is part of qps -- Qt-based visual process status monitor
 *
 * Copyright 2015 Hong Jen Yee (PCMan) <pcman.tw@gmail.com>
 * Copyright 2012-2014 Andriy Grytsenko (LStranger) <andrej@rep.kiev.ua>
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

// for Non-ASCII locale
//#include <global.h>

//#include <qtextcodec.h>
// extern QTextCodec * codec ;
//#define UniString(str)   codec->toUnicode(str)

int flag_24_ok; // we presume a kernel 2.4.x

#include <grp.h>
#include <pwd.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

// COMMON
bool Procview::treeview = 0; // true
bool Procview::flag_show_file_path = false;
bool Procview::flag_cumulative = false;  // times cumulative with children's
bool Procview::flag_pcpu_single = false; // %CPU= pcpu/num_cpus

// COMMON: basic field
int Procview::mini_fields[] = {F_PID, F_STAT, F_MEM, F_CPU, F_CMDLINE, F_END};

// COMMON?
// return username from /etc/passwd
char *userName(int uid, int euid)
{
    char buff[128];
    struct passwd *pw = getpwuid(uid);
    if (!pw)
    {
        // dont have name !
        sprintf(buff, "%d", uid);
    }
    else
        strcpy(buff, pw->pw_name);

    if (uid != euid)
        strcat(buff, euid == 0 ? "*" : "+");

    return strdup(buff);
}

// return group name (possibly numeric)
char *groupName(int gid, int egid)
{
    char *p;
    struct group *gr = getgrgid(gid);
    if (!gr)
    {
        p = (char *)malloc(11);
        sprintf(p, "%d", gid);
    }
    else
        p = strdup(gr->gr_name);
    //	s.append("*");
    return p;
}

Category::~Category() {}

int Category::compare(Procinfo *a, Procinfo *b)
{
    return string(a).compare(string(b));
}

Cat_int::Cat_int(const QString &heading, const QString &explain, int w,
                 int Procinfo::*member)
    : Category(heading, explain), int_member(member), field_width(w)
{
}

QString Cat_int::string(Procinfo *p)
{
    QString s;
    s.setNum(p->*int_member);
    return s;
}

int Cat_int::compare(Procinfo *a, Procinfo *b)
{
    // qsort() only cares about the sign of the number returned by the
    // comparison function; only a subtraction is necessary
    return a->*int_member - b->*int_member;
}

// COMMON
Cat_percent::Cat_percent(const QString &heading, const QString &explain, int w,
                         float Procinfo::*member)
    : Category(heading, explain), float_member(member), field_width(w)
{
}

QString Cat_percent::string(Procinfo *p)
{
    QString s;
    s.sprintf("%01.2f", (double)(p->*float_member));
    return s;
}

int Cat_percent::compare(Procinfo *a, Procinfo *b)
{
    float at = a->*float_member, bt = b->*float_member;
    return at < bt ? 1 : (at > bt ? -1 : 0);
}
// added 2006/05/07
Cat_memory::Cat_memory(const QString &heading, const QString &explain, int w,
                       unsigned long Procinfo::*member)
    : Category(heading, explain), uintl_member(member), field_width(w)
{
}

QString Cat_memory::string(Procinfo *p)
{
    QString s;
    char buff[128];

    long sizeK, sizeM;
    sizeK = p->*uintl_member;

    sizeM = sizeK / 1024;
    /*	if ( sizeM > 1024 )
            {
                    sprintf(buff,"%dM",sizeM/1024);
            }
            else */
    if (sizeM > 0)
    {
        sprintf(buff, "%ldM", sizeM);
    }
    else
        sprintf(buff, "%ldK", sizeK);

    if (sizeK == 0)
        s = "0";
    else
        s = buff;
    return s;
}

int Cat_memory::compare(Procinfo *a, Procinfo *b)
{
    int bu = b->*uintl_member, au = a->*uintl_member;
    return bu >= au ? (bu == au ? 0 : 1) : -1;
}

Cat_uintl::Cat_uintl(const QString &heading, const QString &explain, int w,
                     unsigned long Procinfo::*member)
    : Category(heading, explain), uintl_member(member), field_width(w)
{
}

QString Cat_uintl::string(Procinfo *p)
{
    QString s;
    s.setNum(p->*uintl_member);
    return s;
}

int Cat_uintl::compare(Procinfo *a, Procinfo *b)
{
    int bu = b->*uintl_member, au = a->*uintl_member;
    return bu >= au ? (bu == au ? 0 : 1) : -1;
}

Cat_hex::Cat_hex(const QString &heading, const QString &explain, int w,
                 unsigned long Procinfo::*member)
    : Cat_uintl(heading, explain, w, member)
{
}

// QString.sprintf 2x speed than glibc.sprintf (by fasthyun@magicn.com)
// but QString structuring & destructring eat more time
QString Cat_hex::string(Procinfo *p)
{
    QString s;
    s.sprintf("%8x", (unsigned)(p->*uintl_member));
    return s;
}

// COMMON,
Cat_swap::Cat_swap(const QString &heading, const QString &explain)
    : Category(heading, explain)
{
}

QString Cat_swap::string(Procinfo *p)
{
    QString s;
    // It can actually happen that size < resident (Sun under Solaris 2.6)
    // Possible with Linux ?

    long sizeK, sizeM;
    sizeK = p->size > p->resident ? p->size - p->resident : 0;

    sizeM = sizeK / 1024;

    if (sizeM > 0)
        s = QString::number(sizeM) + "M";
    else
        s = QString::number(sizeK) + "K";

    if (sizeK == 0)
        s = "0";
    return s;
}

int Cat_swap::compare(Procinfo *a, Procinfo *b)
{
    return (b->size - b->resident) - (a->size - a->resident);
}

Cat_string::Cat_string(const QString &heading, const QString &explain,
                       QString Procinfo::*member)
    : Category(heading, explain), str_member(member)
{
}

QString Cat_string::string(Procinfo *p) { return p->*str_member; }

Cat_user::Cat_user(const QString &heading, const QString &explain)
    : Cat_string(heading, explain)
{
}

QString Cat_user::string(Procinfo *p)
{
    if (p->uid == p->euid)
        return Uidstr::userName(p->uid);
    else
    {
        QString s = Uidstr::userName(p->uid);
        s.append(p->euid == 0 ? "*" : "+");
        return s;
    }
}

Cat_group::Cat_group(const QString &heading, const QString &explain)
    : Cat_string(heading, explain)
{
}

QString Cat_group::string(Procinfo *p)
{
    if (p->gid == p->egid)
        return Uidstr::groupName(p->gid);
    else
    {
        QString s = Uidstr::groupName(p->gid);
        s.append("*");
        return s;
    }
}

Cat_wchan::Cat_wchan(const QString &heading, const QString &explain)
    : Cat_string(heading, explain)
{
}

QString Cat_wchan::string(Procinfo *p)
{
#ifdef LINUX
    return p->wchan_str;
#else
    return Wchan::name(p->wchan);
#endif
}

Cat_cmdline::Cat_cmdline(const QString &heading, const QString &explain)
    : Cat_string(heading, explain)
{
}

QString Cat_cmdline::string(Procinfo *p)
{
    if (p->cmdline.isEmpty())
    {
        QString s("(");
        s.append(p->command);
        s.append(")");
        return s;
    }
    else
    {
        if (Procview::flag_show_file_path)
            return p->cmdline;
        else
        {
            QString s(p->cmdline);

            int i = s.indexOf(' ');
            if (i < 0)
                i = s.length();
            if (i > 0)
            {
                i = s.lastIndexOf('/', i - 1);
                if (i >= 0)
                    s.remove(0, i + 1);
            }
            return s;
        }
    }
}

// hmm COMMON? almost same...but Solaris's zombie process...
Cat_start::Cat_start(const QString &heading, const QString &explain)
    : Category(heading, explain)
{
}
// don't let your confidence be shaken, just learn what you can from this young
// master.
QString Cat_start::string(Procinfo *p)
{
#ifdef SOLARIS
    if (p->state == 'Z')
        return "-"; // Solaris zombies have no valid start time
#endif
    Proc *proc = p->proc;
    //	time_t start = boot_time + p->starttime / (unsigned)HZ;
    QString s;
    // time_t start = proc->boot_time + p->starttime / proc->clk_tick;
    time_t start = p->starttime;
    char *ct = ctime(&start);                    // ctime(sec)
    time_t sec = p->starttime - proc->boot_time; // secs
    // if(p->tv.tv_sec - start < 86400) {
    if (sec < 86400)
    { // 24hours
        ct[16] = '\0';
        s = ct + 11; // Hour:minu
    }
    else
    {
        ct[10] = '\0';
        s = ct + 4; // Date
    }
    return s;
}

int Cat_start::compare(Procinfo *a, Procinfo *b)
{
    unsigned long bs = b->starttime, as = a->starttime;
    return bs >= as ? (bs == as ? 0 : 1) : -1;
}

/* ========================  Procview ============================  */
float Procview::avg_factor = 1.0;

Procview::Procview()
{
    // Proc::Proc(); //call once more ?
    cats.clear();
    reversed = false;
    viewproc = ALL;
    treeview = true;
    idxF_CMD = 0; // **** important ****
    viewfields = USER;
    set_fields();

    sortcat = 0; // categories[F_PID];
    sort_column = -1;
    sortcat_linear = NULL;
    enable = true;
    maxSizeHistory = 1200;
    refresh(); // before reading Watchdog list
}

// COMMON:
// Description :
// 		View mode : ALL , OWNER, NO-ROOOT , HIDDEN, NETWORK
bool Procview::accept_proc(Procinfo *p)
{
    QString pid;
    static int my_uid = getuid();
    bool result;

    result = true;

		// BAD
    if (false and viewproc == NETWORK)
    {
        /*
        p->read_fds();
        if(p->sock_inodes.size()==0)
                result=false;
        for(int i=0;i<p->sock_inodes.size();i++)
        {
                SockInode *sn=p->sock_inodes[i];
                Sockinfo *si=Procinfo::socks.value(sn->inode,NULL);
                if(si)
                {
                        si->pid=p->pid;
                        linear_socks.append(si);
                }
        }
        */
    }
    else if (viewproc == ALL)
        result = true;
    else if (viewproc == OWNED)
        result = (p->uid == my_uid);
    else if (viewproc == NROOT)
        result = (p->uid != 0);
    else if (viewproc == RUNNING)
        result = strchr("ORDW", p->state) != 0;

    /*
    if ( viewproc == HIDDEN)
    {
            result=false;
            for(int j=0;j<hidden_process.size();j++)
                    if(hidden_process[j]==p->command)
                            result=true ;
    }
    else
    {
            for(int j=0;j<hidden_process.size();j++)
                    if(hidden_process[j]==p->command)
                            result=false;
    } */

    if (result == false)
        return false; // dont go further !! for better speed

    /// if(search_box==NULL)		return result;
    if (filterstr == "")
        return result;

    if (filterstr == "*")
        return true;

    // Notice:
    //  1. some process name maybe different CMDLINE.  ex) Xorg
    if (p->cmdline.contains(filterstr,
                            Qt::CaseInsensitive)) // search_box->text()
        return true;
    if (p->command.contains(filterstr, Qt::CaseInsensitive))
        return true;
    if (p->username.contains(filterstr, Qt::CaseInsensitive))
        return true;

    pid = pid.setNum(p->pid); //=QString::number(p->pid);
    if (pid.contains(filterstr, Qt::CaseSensitive))
        return true;
		//show all threads for specified PID
    pid = pid.setNum(p->tgid); //=QString::number(p->pid);
    if (pid.contains(filterstr, Qt::CaseSensitive))
        return true;

    /// printf("search_Box =%s , %s
    /// \n",search_box->text().toAscii().data(),p->command.toAscii().data());

    return false;
}

extern "C" {
typedef int (*compare_func)(const void *, const void *);
}

/*
template<class T>
void Svec<T>::sort(int (*compare)(const T *a, const T *b))
{
    qsort(vect, used, sizeof(T), (compare_func)compare);
}
*/

// table view - sort
void Procview::linearize_tree(QVector<Procinfo *> *ps, int level, int prow,
                              bool hide)
{
    static_sortcat = sortcat;
    // ps->sort(reversed ? compare_backwards : compare);
    if (reversed)
        qsort(ps->data(), ps->size(), sizeof(Procinfo *),
              (compare_func)compare_backwards);
    else
        qsort(ps->data(), ps->size(), sizeof(Procinfo *),
              (compare_func)compare);

    int size = ps->size();
    // printf("level=%d prow=%d size=%d\n",level,prow,size);
    for (int i = 0; i < size; i++)
    {
        Procinfo *p = (*ps)[i];
        //	if (p->pid<6 )	printf("pid=%d level=%d
        // parent_row=%d\n",p->pid,p->pid,p->parent_row);
        p->level = level;
        p->lastchild = false;
        p->table_child_seq = i; // ************* where using ? -> sequence
        p->parent_row = prow;
        if (!hide)
            linear_procs.append(p);   // need !!
        if (p->table_children.size()) // and !p->hidekids)
            linearize_tree(&p->table_children, level + 1,
                           linear_procs.size() - 1, hide | p->hidekids);
    }

    if (size > 0)
        (*ps)[size - 1]->lastchild = true;
}

/// basic,memory,job fields
void Procview::set_fields_list(int fields[])
{
    cats.clear();
    for (int i = 0; fields[i] != F_END; i++)
    {
        if (fields[i] == F_CPUNUM and Proc::num_cpus < 2)
            continue; // not correctly work

        Category *c = categories.value(fields[i], NULL);
        if (c)
        {
            // printf("name=%s\n",c->name);
            cats.append(c);
        }
    }
    return;
}

// return the column number of a field, or -1 if not displayed
int Procview::findCol(int field_id)
{
    for (int i = 0; i < cats.size(); i++)
        if (cats[i]->id == field_id)
            return i;
    return -1;
}

// basis
// called by
// 	void Procview::fieldArrange();
// 	qps();
//
void Procview::addField(int Fid, int where)
{
    //	where=-1;
    //	printf("Fid =%d where=%d\n",Fid,where);
    //	where=pstable->clickedColumn();
    if (where == 0)
    {
        // if (pstable->treeMode())  where=1;
    }

    if (Fid == F_PROCESSNAME)
        where = 0;
    if (Fid == F_CMDLINE)
        where = cats.size(); // always should be the last column

    if (where < 0)
    {
        // this is default
        where = cats.size();
    }

    if (where > cats.size()) // CMD_LINE ! ??????
        where = 1;

    Category *newcat = categories[Fid];
    //	printf("name =%s where=%d\n",newcat->name,where);
    if (cats.indexOf(newcat) < 0) // if not in the list ****
        cats.insert(where, newcat);
}

// add a category to last by name
void Procview::addField(char *name) // interface
{
    // QString str=sl[i];
    int id = field_id_by_name(name);
    if (id >= 0)
        addField(id); // add to last

    return;

    Category *cat = cat_by_name(name);
    if (cat)
        cats.append(cat);
}

void Procview::removeField(int field_id)
{
    for (int i = 0; i < cats.size();)
    {
        //	 	printf("Fid=%d cats[%d].id=%d
        //\n",field_id,i,cats[i]->id);
        if (true)
        {
            if (cats[i]->id == field_id)
            {
                //	if(cats[i]->id==F_CMD and !treeview)
                // idxF_CMD=-1;
                cats.remove(i);
                break;
            }
            else
                i++;
        }
    }
    //	printf("Fid=%d cats.size=%d\n",field_id,cats.size());
}

//	called by
//		1. write_settings()
//	DEL? -> not yet
void Procview::update_customfield()
{
    int i;
    //	printf("update custom_fields\n");

    customfields.clear();

    for (i = 0; i < cats.size(); i++)
        customfields.append(cats[i]->name);

    // DEL
    if (false and treeview)
    {
        int idx = customfields.indexOf("COMMAND"); // == removeField(F_CMD);
        if (idx >= 0)
            customfields.removeAt(idx);

        if (idxF_CMD >= 0)
        {
            /* if(customfields.size()<=idxF_CMD)
                    customfields.append(categories[F_CMD]->name);
            else	*/
            customfields.insert(idxF_CMD, categories[F_CMD]->name);
        }
    }

    return;
}

// Description : FIELD movement by mouse drag
//				  From col To place
void Procview::moveColumn(int col, int place)
{
    int i;

    int f_size = cats.size();
    if (col < 0 or place < 0 or f_size <= col or f_size <= place)
    {
        printf("QPS code bugs!! : moveColumn() col=%d place=%d "
               "cats.size=%d\n",
               col, place, cats.size());
        return;
    }

    if (treeview == true)
    {
        // *** important : F_PROCESSNAME field should be the first in
        // TreeMode!
        /// if(place==0)	return;
        if (cats[col]->index == F_PROCESSNAME)
            place = 0;
    }

    // COMMAND_LINE field should always be the last field
    // if(cats[place]->index==F_CMDLINE) return;
    if (cats[col]->index == F_CMDLINE)
        place = cats.size() - 1;

    Category *cat = cats[col]; // SEGFAULT POSSIBLE!
    cats.insert(place, cat);   // insert

    if (place < col)
        col++;

    cats.remove(col); // remove idx
                      // refresh();
                      /// update_customfield();
}

// always called when linear to tree
// DEL
void Procview::saveCOMMANDFIELD() {}

// call by
//		void Pstable::moveCol(int col, int place)
// 		void Pstable::setTreeMode(bool treemode)
//		void Procview::set_fields()
//	TODO: checkField();
void Procview::fieldArrange()
{

    if (treeview == true)
    {
        // 	Tree Mode
        // 	If ProcessName isn't the leftmost column, move it to
        // leftmost
        // *** important : F_PROCESSNAME field should be the first in
        // TreeMode!
        if (cats[0]->index != F_PROCESSNAME)
        {
            // find F_PROCESSNAME
            for (int i = 1; i < cats.size(); i++)
            {
                if (cats[i]->index == F_PROCESSNAME)
                    moveColumn(i, 0);
            }
        }
        // PID sort for convenience (default)
        /*
        if(false and cats[i]->index == F_PID )
        {
                reversed = false;
                sortcat = cats[i];
                //// pstable->setSortedCol(i);
        }	*/
        // Linear_Mode:
    }

    if (true)
    {

        for (int i = 0; i < cats.size(); i++)
        {

            if (cats[i]->index == F_CMDLINE)
            {
                // COMMAND_LINE field should always be the last
                // field
                if (i == (cats.size() - 1))
                    moveColumn(i, 0);
                else
                {
                }
            }
        }
    }
}

void Procview::setTreeMode(bool b)
{
    if (treeview == false)
    {
        if (sortcat_linear == NULL)
            sortcat_linear = sortcat;
        else
            sortcat = sortcat_linear;
    }
}

void Procview::setSortColumn(int col, bool r)
{
    // qDebug("xxx error? col>=cats.size() %d",col);
    if (col >= cats.size())
    {
        qDebug("hmmm error? col>=cats.size() %d", col);
        return;
    }

    // if(!procview->treeview) just reverse the lines
    if (col == sort_column)
        reversed = !reversed;
    else
        reversed = false;
    sortcat = cats[col];
    sort_column = col;
}

Category *Procview::static_sortcat = 0;
int Procview::compare(Procinfo *const *a, Procinfo *const *b)
{
    if (static_sortcat == 0)
        return 0;
    int r = static_sortcat->compare(*a, *b);
    return (r == 0) ? ((*a)->pid > (*b)->pid ? 1 : -1) : r;
}

int Procview::compare_backwards(Procinfo *const *a, Procinfo *const *b)
{
    if (static_sortcat == 0)
        return 0;
    int r = static_sortcat->compare(*b, *a);
    return (r == 0) ? ((*b)->pid > (*a)->pid ? 1 : -1) : r;
}

// COMMON
Category *Proc::cat_by_name( const QString &s )
{

    if ( ! s.isNull() )
    {
        // java style
        QHashIterator<int, Category *> i(categories);
        while (i.hasNext())
        {
            i.next();
            const QString &p = i.value()->name;
            int index = p.indexOf( QRegExp( "\\S" ) );
            if ( p.indexOf( s, index ) == index )
                return i.value();
            //     cout << i.key() << ": " << i.value() << endl;
        }
    }
    return 0;
}

// COMMON
// call by
int Proc::field_id_by_name(const QString &s)
{
    if ( ! s.isNull() )
    {
        // STL style
        QHash<int, Category *>::iterator i = categories.begin();
        while (i != categories.end())
        {
            if ( i.value()->name == s )
                return i.key(); // cout << i.key() << ": " <<
                                // i.value() << endl;
            ++i;
        }
    }
    return -1;
}

// postInit
void Proc::commonPostInit()
{
    QHash<int, Category *>::iterator i = categories.begin();
    while (i != categories.end())
    {
        i.value()->index = i.key();
        i.value()->id = i.key();
        ++i;
    }

    num_opened_files = 0;    // test
    num_process = 0;         //	64bit
    num_network_process = 0; // 64bit

    dt_total = 0; // diff system tick
    dt_used = 0;  // for infobar

    loadavg[0] = loadavg[1] = loadavg[2] =
        0.0; // CPU load avg 1min,5min,15minute

    Proc::num_cpus = 0;
    Proc::old_num_cpus = 0;

    Proc::mem_total = 0;
    Proc::mem_free = 0;

    Proc::mem_buffers = 0;
    Proc::mem_cached = 0;

    Proc::swap_total = 0;
    Proc::swap_free = 0;

    Proc::qps_pid = -1;
    Proc::loadQps = 0.0;

    Proc::cpu_times_vec = 0; // array.
    Proc::old_cpu_times_vec = 0;

    Proc::boot_time = 0;
    Proc::clk_tick = 100; // for most system

    current_gen = 0; // !
    mprocs = NULL;
    hprocs = NULL;

    clk_tick =
        sysconf(_SC_CLK_TCK); //****** The  number  of  clock ticks per second.
                              //
                              // printf("Qps: hz=%d\n",clk_tick);

    ///	Procinfo::init_static();
}

// COMMON?
// 	called by Procview::rebuild()
// 	using procs, makes root_procs for TABLE_View
void Procview::build_tree(Proclist &procs)
{
    Procinfo *p;
    int proc_n = 0;

    // children clear
    root_procs.clear();

    QHash<int, Procinfo *>::iterator i;
    for (i = procs.begin(); i != procs.end();)
    {
        Procinfo *p = i.value();
        p->table_children.clear();
        if ((p->accepted = accept_proc(p)))
            proc_n++;
        ++i;
    }

    Proc::num_process = proc_n; // count process

    // find parent of a process
    for (i = procs.begin(); i != procs.end(); ++i)
    {
        p = i.value(); // always not NULL

        if (p->accepted)
        {
            Procinfo *parent = 0;
            int virtual_parent_pid;

            if (p->isThread())
                virtual_parent_pid = p->tgid; // thread's leader ID.
            else
                virtual_parent_pid = p->ppid;

            if (p->pid < p->ppid)
            {
                // this occurs !! a reporter mailed to me
            }

            parent = procs.value(virtual_parent_pid,
                                 NULL); // if pid not found, then return NULL.

            //	printf("thread_leader=0 (%d)
            //%s\n",p->tgid,p->command.ascii());
            if (treeview and parent and parent->accepted)
            {
                // p->table_child_seq=parent->table_children.size();
                parent->table_children.append(p);
            }
            else
            {
                // 1.init(pid=1) process
                // 2.some process which parent not accepted
                // 3.(null) thread has TGID=0,PPID=0
                // 4.when not tree mode
                root_procs.append(p);
            }
        }
    }
}

// COMMON
// BOTTLENECK No.3   0.5%
// re-sort the process list , ::rebuild() for Table
// called by Pstable::refresh()
void Procview::rebuild()
{
    linear_procs.clear(); // Svec<Procinfo *> procs in Proview
#ifdef LINUX
    linear_socks.clear();
#endif

    // Procinfo *pi = getProcinfoByPID(Procinfo::qps_pid);
    // if(pi) Procinfo::loadQps=pi->pcpu;
    //	printf("rebuild\n");

    /* if(mprocs)
    {
            build_tree(*mprocs);
    }
    else */
    build_tree(Proc::procs);

    linearize_tree(&root_procs, 0, -1);
}

#include <math.h> // log()
// COMMON: CORE
// Description: update the process list      BottleNeck 1.5%
// 		read /proc/*
// 		called by Procview::refresh(), every UPDATE .
void Proc::refresh()
{
    current_gen++;

    // TEST for Process History
    SysHistory *s = new SysHistory;

    history.append(s);
    hprocs = &(s->procs);

    // init
    /// num_opened_files=0;
    Proc::num_process = 0;
    Proc::num_network_process = 0;

    Proc::read_loadavg();
    Proc::read_system(); // **** should be every refresh !!
    // s->load_cpu=(float)Proc::dt_used/Proc::dt_total;  //after
    // read_system();

    s->load_cpu = load_cpu; // after read_system();
    s->time = time(NULL);   // save current time in seconds since epoch

    // TODO: clean
    // Procinfo::read_sockets(); // for future, BottleNect 2%
    read_byte = 0;
    write_byte = 0;
    io_byte = 0; //!!!
                 ///	Proc::read_sockets(); // test

    read_proc_all(); // Linux, Solaris...

    // s->load_io=(float)(read_byte + write_byte ); // (50*1024*1024);
    // //dt_total;
    if (io_byte != 0)
    {
        s->load_io = log10(io_byte) * 2; // 9000 -> 3point
        //	printf("DEBUG: rw=[%d,%d] %d, %f\n",read_byte,
        // write_byte,io_byte,s->load_io);
    }
    else
        s->load_io = 0;

    // remove non-existing processes, remove Procinfos of nonexisting
    // processes
    QHash<int, Procinfo *>::iterator i;
    for (i = procs.begin(); i != procs.end();)
    {
        Procinfo *p = i.value();
        if (p->generation != current_gen)
        {
            //			printf("delete %d\n",p->pid);
            i = procs.erase(i);
            delete p;
        }
        else
        {
            //	p->table_children.clear();  // for rebuild()
            //	p->accepted=accept_proc(p);
            ++i;
        }
    }

    //	TESTING
    while (history.size() >= maxSizeHistory)
    {
        // if(history.isEmpty()==false)
        delete history.takeFirst();
    }
}

// COMMON
// read new process info
void Procview::refresh()
{
    /****************************************************************/
    /* Procview.procs has the procinfo
     */
    /****************************************************************/
    if (enable)
    {
        // printf("Procview::refresh()\n");
        Proc::refresh(); // read "/proc/*", then update the process list
    }
}

SysHistory::~SysHistory()
{
    for (const auto *p : qAsConst(procs))
    {
        delete p;
    }
    procs.clear(); // remove all (key,val)
}

/* check
Category *Proc::cat_by_name(const char *s)
{
    if( s )
    {
        for( int i = 0; i < categories.size(); i++ )
            if( strcmp(categories[i]->name, s) == 0 )
                return categories[i];
    }
    return 0;
}


int  Proc::field_id_by_name(const char *s)
{
    if( s )
    {
        for( int i = 0; i < categories.size(); i++ )
            if( strcmp(categories[i]->name, s) == 0 )
                return i;
    }
    return -1;
}
*/
