/*
 * htable.cpp
 * This file is part of qps -- Qt-based visual process status monitor
 *
 * Copyright 1997-1999 Mattias Engdegård
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


//	very dirty and  complex code ( fasthyun@magicn.com)
//	Disaster !!

// TODO:
// * autoscroll speed proportional to distance from edge
// * interface to add/remove rows (for disclosure triangles)
// * interface to display pixmaps in cells (for disclosure triangles etc)
// * include sorting functionality here for more generality

#include "htable.h"
#include <QMouseEvent>
#include <QStyleOption>
#include <QShortcut>
#include <QPalette>
#include <QApplication>

#define DEBUG qDebug
//#define DEBUG
VPointer *vp = NULL; // Temporary

VPointer::VPointer(QWidget *parent) : QWidget(parent)
{
    pix = new QPixmap(":/icon/vpointer.png");
    pix_vcross = new QPixmap(":/icon/vcross.png");
    int w = pix->width();
    bool flag_movable = true;
    setGeometry(0, 0, w, w);
}

void VPointer::paintEvent(QPaintEvent *event)
{
    QPainter p(this);
    p.setClipping(false); // not work
    if (flag_movable)
        p.drawPixmap(0, 0, *pix);
    else
        p.drawPixmap(0, 0, *pix_vcross);
}

FloatingHead::FloatingHead(QWidget *parent) : QWidget(parent) {}

void FloatingHead::setTitle(QString str, int w, int h)
{
    title = str;
    resize(w, h);
}
void FloatingHead::paintEvent(QPaintEvent *event)
{
    QPainter p(this);
    QStyleOptionHeader opt;
    p.setOpacity(0.7);
    opt.rect = rect();
    opt.text = title;
    opt.state = QStyle::State_Enabled;
    // if(htable->sortedCol()==col) opt.state= opt.state |
    // QStyle::State_Sunken;
    // CE_Header, CE_HeaderSection, CE_HeaderLabel
    style()->drawControl(QStyle::CE_Header, &opt, &p, this);
    return;
}

// TableHead: the horizontally scrollable table head
TableHead::TableHead(HeadedTable *parent)
    : QtTableView(parent), htable(parent), dragging(false)
{
    setSizePolicy(QSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum));
    // setSizePolicy(QSizePolicy
    // (QSizePolicy::Minimum,QSizePolicy::Expanding));
    // setTableFlags(Tbl_smoothHScrolling | Tbl_scrollLastHCell);
    setTableFlags(Tbl_scrollLastHCell); // ?
    setNumRows(1);

    setMouseTracking(true); // for tooltip

    floatHead = new FloatingHead(parent); // tiny memory leak! don't care.
    floatHead->hide();
    // setMinimumHeight(20);//cellHeight()); //ZERO!! fault
}

// Description : draw a field name of table  when DRAG !!
// 		called by paintHeading()
// 		called by QtTableView::repaint()
void TableHead::paintCell(QPainter *p, int row, int col, bool use_cache)
{
    static int count = 0;
    /// printf(" paintcell %d,%d\n",row,col);
    int w = htable->max_widths[col];

    // if( isCellChanged (row,col,true)==false)
    {
        //	printf(" paintcell %d,%d\n",row,col);
        //	if(use_cache==true)  // check cached value
        {
            //	return;
        }
    }

    // if( col ==11 )	printf("head: %d  (%d) \n",col,count++);
    // w = cellUpdateR.width();
    {
        QStyleOptionHeader opt;
        QRect rectR(0, 0, w, height());
        opt.rect = rectR;
        opt.text = htable->title(col);
        opt.state = QStyle::State_Enabled;
        if (htable->sortedCol() == col)
            opt.state = opt.state | QStyle::State_Sunken;
        // CE_Header, CE_HeaderSection, CE_HeaderLabel
        style()->drawControl(QStyle::CE_Header, &opt, p, this);
    }
}

// works
void TableHead::eraseRight(QPainter *p, QRect &r)
{
    QStyleOptionHeader opt;
    //		printf("widt=%d \n",r.width());
    opt.rect = r;
    opt.text = "";
    style()->drawControl(QStyle::CE_Header, &opt, p, this);
}

// virtual !
int TableHead::cellWidth(int col) { return htable->max_widths[col]; }

//
void TableHead::scrollSideways(int val) { setXOffset(val); }

void TableHead::mousePressEvent(QMouseEvent *e)
{
    int col = findCol(e->x());
    //	printf("col=%d\n",col);
    if (col == -1)
        return;
    click_col = col;

    if (e->button() == Qt::RightButton)
    // && htable->options & HTBL_HEADING_CONTEXT_MENU)
    {
        emit rightClicked(e->globalPos(), col);
        return;
    }

    if (e->button() == Qt::LeftButton)
    {
        press = e->pos();
    }
}

void TableHead::dragEnterEvent(QDragEnterEvent *event)
{
    printf("dragEnterEvent()\n");
}
void TableHead::dragLeaveEvent(QDragLeaveEvent *event)
{
    printf("dragLeaveEvent()\n");
}

void TableHead::mouseMoveEvent(QMouseEvent *e)
{
    //	printf("mouseMoveEvent()\n");
    int col = findCol(e->x());
    if (col < 0)
    {
        emit htable->outOfHCell();
        return;
    }
    //	if( !dragging) return;
    emit htable->flyOnHCell(col);

    int thold = abs(press.x() - e->pos().x());

    if (e->buttons() == Qt::LeftButton //  Button state
        and htable->options & HTBL_REORDER_COLS)
    {
        {

            if (!dragging and thold > 5)
            {
                dragging = true;
                drag_offset =
                    press.x() - htable->colOffset(click_col) + xOffset();
                floatHead->setTitle(htable->title(click_col),
                                    cellWidth(click_col), height());
                floatHead->show();
                // vp->show();
            }

            if (dragging)
            {
                //	htable->body->drawGhostCol(drag_pos -
                // drag_offset, w);
                floatHead->move(e->x() - drag_offset, 0);

                int vcol = findColNoMinus(e->x() - drag_offset);
                int vcol_offx = htable->colOffset(vcol);
                static int old_pos = -1;
                //	printf("vcol=%d\n",vcol);

                // virtual Pointer move!
                if (old_pos != vcol_offx)
                {
                    QWidget *p = parentWidget(); // htable
                    //	vp->setMovable(htable->columnMovable(htable->columnMovable(vcol)
                    // and htable->columnMovable(click_col)
                    // ));
                    //	vp->move(vcol_offx -
                    // p->geometry().x()
                    //,p->geometry().y() -
                    // 13);
                    // vp->move(e->x()	,10);
                    old_pos = vcol_offx;
                }
                drag_pos = e->x(); // save old pos

                return;
            }
        }
    }

    if (col != -1) // ToolTip
    {
        //	for(int i=0;i<1024*1024;i++)	htable->tipText(col);
        //// memory leak
        // test
        QString s = htable->tipText(col);

        if (!s.isEmpty())
            QToolTip::showText(e->globalPos(), s);
    }
}

void TableHead::mouseReleaseEvent(QMouseEvent *e)
{
    if (e->button() == Qt::LeftButton) // Returns the button that caused the
                                       // event.
    {
        int col = findCol(e->x());
        if (col < 0)
            col = (e->x() < 0) ? 0 : htable->ncols - 1; // good!

        if (!dragging and col == click_col)
        {
            // htable->emit_click_signal(col); // just click no drag
            emit htable->titleClicked(col);
        }

        if (dragging)
        {
            int vcol = findColNoMinus(e->x() - drag_offset);
            if (click_col >= 0)
            {
                htable->moveCol(click_col, vcol); // call
                                                  // Pstable::moveCol(int
                                                  // col, int place)
                                                  // should emit !!
                // emit colMoved(click_col, vcol);
            }
            ///	printf(" vcol2=%d \n",vcol);
        }
        dragging = false;
        floatHead->hide();
        // vp->hide();
    }
}

#include <QStyleFactory>

// TableBody: the table body, scrollable in all ways
TableBody::TableBody(HeadedTable *parent) : QtTableView(parent), htable(parent)
{

    // setTableFlags(Tbl_autoScrollBars | Tbl_smoothScrolling);
    setTableFlags(Tbl_snapToVGrid); // works
    setTableFlags(Tbl_hScrollBar | Tbl_vScrollBar);
    first_drag_row = prev_drag_row = -1;
    autoscrolling = false;
    gadget_click = false;
    setMouseTracking(true);

    QStyle *s = QStyleFactory::create("windows");
    if (s)
        setStyle(s);
}

// virtual from QtTableView
void TableBody::scrollTrigger(int rx, int ry)
{
    if (ry == 0)
        return;
    int n = ry / cellHeight();
    /// printf("ry=%d n=%d\n",ry,n);
    if (ry > 0)
        tablecache.cutDown(n); // error?
    else
        tablecache.cutUp(-n);
    //	printf("ry=%d cellheight=%d n=%d\n",ry,cellHeight(),n);
}

// new, possible COMMON?
bool TableBody::isCellChanged(int row, int col)
{
    QString str;
    int xpos;
    int fold, dep;
    bool selected;
    bool sorted;
    int lc;
    int width;

    {
        str = htable->text(row, col);
        xpos = htable->colXPos(col);
        selected = htable->isSelected(row);
        sorted = (htable->sortedCol() == col);
        width = htable->max_widths[col];
    }

    // int left = leftCell(), top = topCell();
    CellAttribute *attr = tablecache.value(row - topCell(), col - leftCell());

    if (attr == NULL) // never NULL
    {
        return true;
    }
    else
    {
        bool result = false;
        if (attr->text == str and attr->selected == selected and
            attr->sorted == sorted and attr->xpos == xpos and attr->w == width
            // and attr->size==tmp_size
            and
            (attr->size.height() >= tmp_size.height())
            //	and (attr->size.width()>=tmp_size.width())
            )
        {
            result = false;
        }
        else
        {
            //	attr->text=str;
            //	attr->selected=selected;
            //	attr->sorted=sorted;
            //	attr->xpos=xpos;
            //	attr->size=tmp_size;
            //	attr->w=width;

            //	if (row==29 and col==0)
            //	printf("(%d %d) xpos=%d %d sorted=%d %d sel=%d
            //%d str=%s
            //%s\n",attr->size.height(),tmp_size.height(),
            //			attr->xpos,xpos,attr->sorted,sorted,attr->selected,selected
            //			,qPrintable(attr->text),qPrintable(str));
            //	printf("true (%d
            //%d)\n",row-topCell(),col-leftCell());
            return true;
        }

        if (col == 0)
        {
            dep = htable->rowDepth(row); // folding_level
            fold = htable->folded(row);
            lc = htable->lastChild(row);

            if (attr->depth == dep /* Uninitial*/
                and attr->folded == fold and attr->lastchild == lc)
            {
                // false
            }
            else
            {
                //	attr->depth=dep;
                //	attr->folded=fold;
                result = true;
                //	printf("true (%d
                //%d)\n",row-topCell(),col-leftCell());
            }
        }
        return result;
    }
}

bool TableHead::isCellChanged(int row, int col)
{
    int xpos;
    int ypos;
    int w;
    int fold, dep;
    QString str;
    bool sorted = (htable->sortedCol() == col);

    w = htable->max_widths[col];
    xpos = htable->colXPos(col);

    {
        str = htable->title(col);
    }
    // int left = leftCell();
    // int top	 = topCell();
    CellAttribute *attr = tablecache.value(row - topCell(), col - leftCell());

    if (attr == NULL) // Uninitialed?
    {
        return true;
    }
    else
    {
        bool result = false;
        if (attr->text == str and attr->sorted == sorted and
            attr->size == tmp_size and attr->xpos == xpos
            //	and attr->w==w
            )
        {
            return false;
        }
        else
        {
            attr->text = str;
            attr->sorted = sorted;
            attr->xpos = xpos;
            //		attr->w=w;
            attr->size = tmp_size;
            //			if(head)	printf("head
            // true");
            return true;
        }
        // printf("return  %s x=%d\n",str.toAscii().data(),xpos);
    }
}

#include <Qt>

// DRAFT CODE !!  BOTTLENECK !!!
// Description : draw a cell of table
// 		1. draw the background of a cell
// 		2. draw the text of a cell
//
// called by
// 		1.QtTableView::paintEvent()
void TableBody::paintCell(QPainter *p, int row, int col, bool use_cache)
{
    // *** sequence important !!!
    //	if(isCellChanged(row,col)==false)	{
    // if(use_cache==true)	return;
    //}

    CellAttribute *attr =
        tablecache.value(row - topCell(), col - leftCell()); // save cache
    //	isCellChanged(row,col);

    bool sort;
    int h = cellHeight();
    int w = htable->max_widths[col];
    attr->w = w; //

    QColor baseColor = palette().brush(QPalette::Base).color();
    sort = (col == htable->sorted_col);
    attr->sorted = sort;

    // gridline
    QColor lineColor, textColor;
    QColor altColor = palette().brush(QPalette::AlternateBase).color();
    lineColor.setRgb((baseColor.red() * 0.5 + altColor.red() * 0.5),
                     (baseColor.green() * 0.4 + altColor.green() * 0.6),
                     (baseColor.blue() * 0.4 + altColor.blue() * 0.6));

    //	QStyleOption opt;
    //	opt.rect=QRect(0,0,w,h-1);
    //	opt.state = QStyle::State_Active; //?
    // QStyle::SH_ItemView_PaintAlternatingRowColorsForEmptyArea
    //  const int gridHint =
    //  style()->styleHint(QStyle::SH_Table_GridLineColor,
    //  &opt, this);

    p->setPen(lineColor);
    p->drawLine(0, h - 1, w, h - 1);

    // background
    if (htable->isSelected(row))
    {
        p->fillRect(0, 0, w, h - 1, palette().brush(QPalette::Highlight));
        p->setPen(palette().brush(QPalette::HighlightedText).color()); // text
        attr->selected = true;
    }
    else
    {
        attr->selected = false;
        if (sort)
        {
            p->fillRect(0, 0, w, h - 1, lineColor.dark(101));
        }
        else
        {
            p->fillRect(0, 0, w, h - 1, baseColor);
        }
        // style()->drawPrimitive(QStyle::PE_FrameFocusRect, &opt, p,
        // this);
        p->setPen(palette().brush(QPalette::Text).color()); // text
    }

    // tree gadget , treestep=height
    int gap = h / 4;
    int testFlag = 0;
    if (col == 0 and htable->treemode == true)
    {
        QStyleOption opt;
        int d = htable->rowDepth(row);
        attr->depth = d; //**
        int treestep = htable->treestep;
        if (true /*lines*/)
        {
            /// int dx = folding ? gadget_space : 6;
            for (int level = d, prow = row; level >= 0 and prow >= 0;
                 level--, prow = htable->parentRow(prow))
            {
                if (level == d)
                    continue;
                int x = gap + level * treestep;
                QRect branchR(x, 0, treestep, h);
                opt.rect = branchR;

                if (htable->lastChild(prow) == false)
                {
                    if (testFlag)
                    {
                        p->drawLine(x + treestep / 2, 0, x + treestep / 2, h);
                    }
                    else
                    {
                        opt.state = QStyle::State_Sibling; // | vertical
                                                           // line
                        style()->drawPrimitive(QStyle::PE_IndicatorBranch, &opt,
                                               p, this);
                    }
                }
            }

            QRect branchR(gap + d * treestep, 0, treestep, h);
            opt.rect = branchR;
            if (testFlag)
            {
                int x = gap + d * treestep + treestep / 2;

                p->drawLine(x, h / 2, x + treestep / 2, h / 2);
                if (!htable->lastChild(row))
                {
                    p->drawLine(x, 0, x, h);
                }
                else
                {
                    p->drawLine(x, 0, x, h / 2);
                }
            }
            else
            {
                opt.state = QStyle::State_Item; //?
                if (!htable->lastChild(row))
                    opt.state = opt.state | QStyle::State_Sibling; // |
            }
            attr->lastchild = htable->lastChild(row);
        }

        if (true /*folding*/)
        {
            HeadedTable::NodeState fs = htable->folded(row);
            attr->folded = fs; //**
            if (fs != HeadedTable::Leaf)
            {
                opt.state = opt.state | QStyle::State_Children;
                if (fs != HeadedTable::Closed)
                    opt.state = opt.state | QStyle::State_Open;
            }
            p->save(); // temp , save pencolor
            style()->drawPrimitive(QStyle::PE_IndicatorBranch, &opt, p, this);
            p->restore();
        }
        gap = htable->gadget_space + d * treestep + 1;
    }

    // virtual int Pstable::alignment(int col)
    if (htable->alignment_col[col] == Qt::AlignRight) // using cache
    {
        w -= gap;
        gap = 0;
    }
    // Qt::IncludeTrailingSpaces
    // p->drawText(gap, 0, w , h, Qt::TextIncludeTrailingSpace |
    // Qt::IncludeTrailingSpaces | Qt::AlignVCenter |
    // htable->alignment_col[col],htable->text(row,col));
    // p->drawText(gap, 0, w , h, Qt::TextIncludeTrailingSpace |
    // Qt::AlignVCenter
    // | htable->alignment_col[col],htable->text(row,col));
    p->drawText(gap, 0, w, h, Qt::AlignVCenter | htable->alignment_col[col],
                htable->text(row, col));

    htable->overpaintCell(p, row, col, gap);

    // cache write!
    // attr->xpos=tmp_x ;
    attr->text = htable->text(row, col);
    attr->xpos = htable->colXPos(col);
    attr->size = tmp_size;
}

// DEL , Home key
void TableBody::jumpTop() { setYOffset(0); }

// DEL, End Key
void TableBody::jumpBottom() { setYOffset(maxYOffset()); }

/// !!!!
void TableBody::centerVertically(int row)
{
    int topcell = row - viewHeight() / (cellHeight() * 2);
    setTopCell(qMax(topcell, 0));
    update();
}

//??
void TableBody::showRange(int from, int to)
{
    int h = viewHeight() / cellHeight();
    if (to >= topCell() + h)
        setTopCell(qMax(0, qMin(from, to - h + 1)));
}

// virtual
// called by
//	1.
int TableBody::cellWidth(int col) { return htable->max_widths[col]; }

// **** fix !!
void TableBody::updateRow(int row)
{
    // ??
    // for(int col = 0; col < htable->ncols; col++)
    // updateCell(row, col,false);	updateCell(row, col);
}

void TableBody::mousePressEvent(QMouseEvent *e)
{
    if (numRows() == 0)
        return; // *** prevent out of range

    // printf("mousePressEvent() 1\n");
    static int last_row = -1;

    int row = findRow(e->y());
    if (row == -1)
    {
        //	printf("mousePressEvent\n");
        htable->clearAllSelections();
        if (e->y() >= 0)
            row = numRows(); // if SHIFT+click outside ~
        first_drag_row = prev_drag_row = row;
        return;
    }

    if (!(htable->options & HTBL_ROW_SELECTION))
        return;

    if (e->button() == Qt::LeftButton)
    {
        // folding
        if (htable->treemode && htable->folding &&
            e->x() < htable->gadget_space +
                         htable->treestep * htable->rowDepth(row) &&
            htable->folded(row) != HeadedTable::Leaf)
        {
            emit htable->foldSubTree(row);
            gadget_click = true;
            last_row = row;
            //	clearCache(); // dont use cache
            return;
        }

        if (e->modifiers() & Qt::ShiftModifier)
        {
            if (row < last_row)
                for (int i = row; i < last_row; i++)
                    htable->setSelected(i, true); // virtual
            else
                for (int i = last_row; i <= row; i++)
                    htable->setSelected(i, true); // virtual
        }
        else if (e->modifiers() & Qt::ControlModifier)
        {
            htable->setSelected(row, !htable->isSelected(row));
        }
        else
            htable->selectOnlyOne(row);

        first_drag_row = prev_drag_row = row;

        emit htable->selectionChanged();
    }

    else if (e->button() == Qt::RightButton)
    {
        if (!htable->isSelected(row))
            htable->selectOnlyOne(row);
        emit htable->selectionChanged();
        // better? 	emit htable->rightClicked(e->globalPos());
    }
    last_row = row;
    //	htable->repaint_changed();
    repaintChanged(); // repaint
                      // view->update(); // fast but use more CPU
}

void TableBody::mouseReleaseEvent(QMouseEvent *e)
{
    gadget_click = false;

    if (e->button() == Qt::LeftButton)
    {
        if (autoscrolling)
        {
            ////killTimers();		// no more autoscrolling
            first_drag_row = prev_drag_row = -1;
            autoscrolling = false;
        }
    }
    else if (e->button() == Qt::RightButton)
    {
        QPoint p(6, 3);
        p += e->globalPos();
        emit htable->rightClicked(p);
    }
}

void TableBody::mouseDoubleClickEvent(QMouseEvent *e)
{
    if (e->button() == Qt::LeftButton)
    {
        int row = findRow(e->y());
        if (row != -1)
        {
            if (htable->options & HTBL_ROW_SELECTION &&
                !htable->isSelected(row))
                htable->selectOnlyOne(row);
            // htable->selectionNotify();
            emit htable->selectionChanged();
            //		htable->emit_double_click_signal(row);
            emit htable->doubleClicked(row);
        }
    }
}

// Bottle Neck ? no
void TableBody::leaveEvent(QEvent *) { emit htable->outOfCell(); }

void TableBody::mouseMoveEvent(QMouseEvent *e)
{
    if (numRows() == 0)
        return; // *** prevent out of range

    // DRAFT CODE !
    // Signal : find row, col(field name) emit flyOnEvent Signal
    int row = findRow(e->y());
    int col = findCol(e->x());

    if (row < 0 or col < 0)
    {
        emit htable->outOfCell(); // if...
        return;
    }

    emit htable->flyOnCell(row, col);

    if (e->buttons() == Qt::NoButton)
        return;
    if (e->buttons() == Qt::RightButton)
        return;

    if (e->buttons() & Qt::ControlModifier || gadget_click)
        return;

    if (row != prev_drag_row)
    {
        if (row == -1)
        {
            if (!autoscrolling)
            {
                // dragging outside table, cause scrolling
                scrolldir = (e->y() < 0) ? UP : DOWN;
                //	killTimers();
                //	startTimer(scroll_delay);
                autoscrolling = true;
            }
        }
        else
        {
            ////killTimers();
            autoscrolling = false;
            dragSelectTo(row);
        }
    }
    //	repaintRow(row);
    //	view->update();
    repaintChanged();
}

void TableBody::timerEvent(QTimerEvent *)
{
    // timer not proved !
    return;
    // printf("timer\n");
    if (!autoscrolling)
        return;
    ////killTimers();
    if (scrolldir == UP)
    {
        int top = topCell();
        setTopCell((top > 1) ? top - 1 : 0);
        dragSelectTo(topCell());
    }
    else
    {
        setTopCell(topCell() + 1);
        int bottom = lastRowVisible();
        dragSelectTo((bottom < numRows()) ? bottom : numRows() - 1);
    }
    startTimer(scroll_delay);
}

// change drag selection point from previous drag position to row
void TableBody::dragSelectTo(int row)
{
    int dir = (row > prev_drag_row) ? 1 : -1;
    if ((prev_drag_row - first_drag_row) * dir >= 0)
    {
        // moving away from start point
        for (int i = prev_drag_row + dir; i - dir != row; i += dir)
            htable->setSelected(i, true);
    }
    else
    {
        // moving towards start point
        for (int i = prev_drag_row; i != row; i += dir)
            htable->setSelected(i, false);
    }
    prev_drag_row = row;
    // htable->selectionNotify();
    emit htable->selectionChanged();
}

// heuristic for determining a good XOR color: This is in general a hard
// problem but since we know (most of) the background and the foreground,
// we can try. Note that this function might not return an allocated QColor,
// so it is only useful for XOR drawing.

QColor TableBody::getXorColor()
{
    QColor fg = palette().brush(QPalette::Base).color();

    return fg; /// QColor(0, fg.pixel() ^ backgroundColor().pixel());
}

void TableBody::drawGhostCol(int x, int w)
{
    static QColor xorcol = getXorColor();

    QPainter p(this);

    p.setPen(xorcol);
    // p.setRasterOp(XorROP);
    /// p.drawLine(x, 0, x, height());
    ////p.drawLine(x + w, 0, x + w, height());
}

// HeadedTable: the actually useable class
HeadedTable::HeadedTable(QWidget *parent, int opts) : QWidget(parent)
{
    // meaningless?
    options = HTBL_ROW_CONTEXT_MENU | HTBL_ROW_SELECTION |
              HTBL_ROW_DOUBLE_CLICK | HTBL_HEADING_TOOLTIPS |
              HTBL_HEADING_CONTEXT_MENU | HTBL_HEADING_CLICK |
              HTBL_REORDER_COLS;

    head = new TableHead(this);
    body = new TableBody(this);
    head->setObjectName("head");
    body->setObjectName("body");

    sorted_col = -1;
    treemode = false;
    treestep = 0;
    lines = true;
    folding = true;
    gadget_space = 0;
    nrows = ncols = 0; // hmm...

    // printf("style name=%s \n",qPrintable(style()->objectName()));
    // QApplication::setStyle(new QWindowsStyle);
    head->setFrameShape(QFrame::NoFrame);
    if (style()->objectName().contains("oxygen"))
        ;
    else
    {
        body->setFrameShape(QFrame::NoFrame);
    }

    QVBoxLayout *vlayout = new QVBoxLayout;
    vlayout->setSpacing(0); // distance between Widget and Widget

#if QT_VERSION < 0x040300
    vlayout->setMargin(0); // qt-4.2
#else
    vlayout->setContentsMargins(0, 0, 0, 0); // qt-4.3
#endif

    //	vlayout->setSizeConstraint ( SizeConstraint )
    vlayout->addWidget(head);
    vlayout->addWidget(body);
    setLayout(vlayout);

    // connect keyboard shortcuts
    QShortcut *c;
    c = new QShortcut(Qt::CTRL + Qt::Key_A, this, SLOT(selectAll()));
    // synchronize horizontal scrolling of head and body
    connect(body->horizontalScrollBar(), SIGNAL(valueChanged(int)), head,
            SLOT(scrollSideways(int)));
    fontChange(); // *** need for init
                        //	vp=new VPointer((QWidget *)QObject::parent());
                        //	vp->hide();		//memory leak!!
}

HeadedTable::~HeadedTable() {}

// ok : move to Qttableview
void HeadedTable::fontChange()
{
    // DEBUG("fontChange()\n");
    int fontHeight = QApplication::fontMetrics().height() + 1;
    // printf("cell height =%d \n", cellH);
    if (fontHeight % 2 != 0)
        fontHeight++; // for pretty fold-lines
    head->setCellHeight(fontHeight + 5);
    head->setMaximumHeight(head->cellHeight());
    body->setCellHeight(fontHeight + 2);
    treestep = fontHeight;
    gadget_space = folding ? fontHeight + (fontHeight / 2) : 0;
    //	QWidget::fontChange ( oldFont );
}

// DRAFT  virtual !
void HeadedTable::moveCol(int col, int place)
{
    // go to void Procview::moveColumn(int col, int place)
}

// distance (in table coords) from left table edge of physical column
int HeadedTable::colOffset(int col)
{
    int x = 0;
    for (int c = 0; c < col; c++)
        x += max_widths[c];
    // x += body->cellWidth(c);
    return x;
}

// OPTIZ
inline int HeadedTable::colXPos(int col)
{
    int x = 0; // + coloffset
    for (int c = 0; c < col; c++)
        x += max_widths[c];
    return x;
}

// repaint columns from col0 to col1. If col1 is -1, repaint all
// the way to the right edge of the table.
// called by
//          1.void Qps::update_table(int col)
void HeadedTable::repaintColumns(int col0, int col1)
{
    QRect bvr = body->viewRect();
    QRect hvr = head->viewRect();
    int x0 = colOffset(col0) - body->xOffset();
    if (x0 > hvr.width())
        return;
    if (x0 < 0)
        x0 = 0;
    bvr.setLeft(x0);
    hvr.setLeft(x0);
    if (col1 >= 0)
    {
        int x1 = colOffset(col1) + max_widths[col1] - body->xOffset();
        if (x1 < hvr.width())
        {
            hvr.setRight(x1);
            bvr.setRight(x1);
        }
    }
    head->repaint(hvr);
    body->repaint(bvr);
}

// DEL -> virtual
// 	called by Pstable::setTreeMode(bool)
void HeadedTable::setTreeMode(bool tm)
{
    treemode = tm;
    head->clearCache();
    body->clearCache();
}

// update table Head !!
void HeadedTable::setSortedCol(int col)
{
    {
        int old_sorted = sorted_col;
        sorted_col = col;
        //		printf("old_sorted=%d\n",old_sorted);
        //		printf("sorted_col=%d\n",sorted_col);
        if (old_sorted != -1 && old_sorted < ncols) // ncols bug
            updateHeading(old_sorted);
        if (col != -1 && col < ncols)
            updateHeading(col);
    }
}

// should be virtual. why?
void HeadedTable::clearAllSelections()
{
    for (int row = 0; row < nrows; row++)
        setSelected(row, false);
    body->view->update();
    //	update(); // not work why?
}

// BottleNeck ?
void HeadedTable::selectOnlyOne(int row)
{
    // this apply to the current list only!
    for (int r = 0; r < nrows; r++)
        setSelected(r, r == row);
}

void HeadedTable::selectAll()
{
    for (int r = 0; r < nrows; r++)
        setSelected(r, true);
    emit selectionChanged(); // notwork?
}

// default implementation returns a null string (no tip d)
QString HeadedTable::tipText(int) { return ""; }
char *HeadedTable::total_selectedRow(int col) { return 0; }

void HeadedTable::setNumRows(int rows)
{
    nrows = rows;
    head->setNumRows(1);
    body->setNumRows(rows);
}

void TableCache::setRow(int row)
{
    if (row < 0)
        return;

    int i, size = rows.size();
    if (row >= size)
    {
        //	rows.append(new TableRow);
    }

    if (row < nrow)
    {
        //		printf("row=%d,nrow=%d \n",row,nrow);
        if (size < nrow)
            nrow = size;
        for (i = row; i < nrow; i++)
        {
            for (int j = 0; j < 48; j++)
                rows[i]->cells[j].text = "";
        }
    }
    nrow = row;
}

void TableCache::setCol(int col)
{
    if (col < 0)
        return;
    if (col < ncol)
    {
        //	printf("clear cache\n");
        int i, size = rows.size();
        for (i = 0; i < size; i++)
            for (int j = col; j < ncol; j++)
                rows[i]->cells[j].text = "";
    }
    ncol = col;
}

// TEST
void TableHead::checkProfile()
{
    QRect viewR = viewRect();
    // if(viewR.y()!=0) printf(" qps: ooooohss....\n");
    int maxViewCol = findCol(viewR.width());
    if (maxViewCol < 0)
        maxViewCol = numCols();
    tablecache.setCol(maxViewCol);
    tablecache.setRow(1);
}

// TESt
void TableBody::checkProfile()
{
    htable->checkTableModel(); // important

    QRect viewR = viewRect();
    if (viewR.y() != 0)
        printf(" qps: ooooophss....\n");
    int maxViewRow = findRow(viewR.height());
    if (maxViewRow < 0)
        maxViewRow = numRows();
    int maxViewCol = findCol(viewR.width());
    if (maxViewCol < 0)
        maxViewCol = numCols();
    tablecache.setCol(maxViewCol);
    tablecache.setRow(maxViewRow);
}

void HeadedTable::setNumCols(int cols)
{
    // printf("cols=%d\n",cols);

    ncols = cols;
    // resetWidths();
    for (int i = 0; i < cols; i++)
    {
        alignment_col[i] = alignment(i);
        updateColWidth(i);
    }
    //	printf("ncols=%d\n",ncols);
    head->setNumCols(ncols);
    body->setNumCols(ncols);
}

/*
//virtual
int HeadedTable::colWidth(int col)
{
        int r = numRows();
        for(int i=0;i<r;i++)	text(i,col);
} */

// BOTTLENECK
// find Max width of a column
// called by
//		1.  void HeadedTable::setNumCols(int cols)
int HeadedTable::updateColWidth(int col)
{
    int w = 0;
    int sw = 0;
    int hw = 0;
    int i = 0;
    int rows = numRows();
    bool treecol = (treemode && col == 0);

    if ((w = sizeHintForColumn(col)) < 0) // width
    {
        if (w < 0)
            w = -w; // trick.
        // if(w<0) printf("w = %d\n",w);
        // harsh, if procs more than 1000
        for (i = 0; i < rows; i++)
        {
            sw = fontMetrics().width(text(i, col)) + 10;

            if (treecol)
                sw += treestep * rowDepth(i);
            if (sw > w)
                w = sw;
        }

        if (treecol)
        {
            w += gadget_space;
        }
    }
    // don't forget the width of the heading
    hw = fontMetrics().width(title(col)) + 12;
    // return 0;
    if (hw > w)
        w = hw;

    // sw=fontmetric.width("0") * colWidth(col);
    // if(sw>w) w=sw;

    max_widths[col] = w;
    return 0;
}

void HeadedTable::resetWidths()
{
    for (int i = 0; i < numCols() + 1; i++)
        max_widths[i] = 0;
}

// called by ?
void HeadedTable::repaintAll() // -> repaint()
{
    int count = 0, trow = 0;
    for (int row = 0; row < nrows; row++)
        if (isSelected(row))
        {
            count++;
            trow = row;
        }
    //	body->view->update();
    //	update(); // not work why?

    //	printf("Qps debug: repaintAll()\n");
    //	head->clearCache();
    //	body->clearCache();
    // update();
    body->view->update();
    head->view->update();
    if (count == 1)
        centerVertically(trow);
}

//  PROBLEM : update()  +  repaint() call = draw twice !!!
//	called by pstable::refresh()
//  DEL if (render_area smaller than full_arear ) then force draw
void HeadedTable::repaint_changed()
{
    //	printf("repaint_changed()\n");
    // body->setUpdatesEnabled(true);
    head->repaintChanged();
    body->repaintChanged();
}

void HeadedTable::hideEvent(QHideEvent *event) {}

void HeadedTable::paintEvent(QPaintEvent *e)
{
    static int c = 0;
    //	printf("HeadedTable paintEvent %d\n",c++); //works ?
    //	QWidget::paintEvent(e);
    // body->clearCache();
    //	head->update();
}

void HeadedTable::showEvent(QShowEvent *)
{
    //	printf("HeadedTable showEvent\n"); // works
    //  before paintEvent call
    head->clearCache();
    body->clearCache();
}

void HeadedTable::resizeEvent(QResizeEvent *e)
{
    // DEBUG("resizeEvent() HeadedTable %d\n",head->cellHeight());
    ////	head->repaintRow(0);
    QWidget::resizeEvent(e);
    head->clearCache();
    body->clearCache();
}
