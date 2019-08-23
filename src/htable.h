/*
 * htable.h
 *
 * Copyright 1997-1999 Mattias Engdegård
 * This file is part of qps -- Qt-based visual process status monitor
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

// This file defines the HeadedTable class.
// A HeadedTable is a QTableView with one line of headers over each column.

#ifndef HTABLE_H
#define HTABLE_H

#include "qttableview.h"

#ifndef USING_PCH
#include <QHash>
#include <QToolTip>
#include <QVBoxLayout>
#endif

// table options (bits to be ORed together)
#define HTBL_ROW_SELECTION 1         // rows are selectable
#define HTBL_ROW_DOUBLE_CLICK 2      // rows can be double-clicked
#define HTBL_ROW_CONTEXT_MENU 4      // right button menu on rows
#define HTBL_HEADING_TOOLTIPS 8      // tooltips on headings
#define HTBL_HEADING_CONTEXT_MENU 16 // right button menu on headings
#define HTBL_HEADING_CLICK 32 // able to click on heading (change sort order)
#define HTBL_REORDER_COLS 64  // able to reorder columns

class HeadedTable;

class CellAttribute
{
  public:
    QString text;
    bool selected;
    bool sorted;
    QSize size;
    QColor backColor; // table share
    QColor foreColor; // rows share
    int xpos;         // cols share
    int ypos;         // rows share
    int w;            // cols share
    int depth;        // rows share
    int folded;       //
    int lastchild;    //
    CellAttribute()
    {
        text = "";
        sorted = selected = false;
        xpos = -1, ypos = -1, w = 0, depth = -1, folded = -1;
        lastchild = 0;
    }
};

struct TableRow
{
  public:
    CellAttribute cells[64]; // enough
};

class TableCache
{
    int nrow, ncol;
    QList<TableRow *> rows;

  public:
    TableCache()
    {
        // for(int i=0;i<1;i++) // tmp
        rows.append(new TableRow);
        nrow = 0;
        ncol = 0;
    }

    void reset() {} // clear cache
    void setSize(int row, int col);
    void setRow(int row);
    void setCol(int col);

    CellAttribute *value(int row, int col)
    {
        if (row < 0)
            qFatal("Qps: Error ! TableCache() under size !! row=%d \n", row);

        if (row >= rows.size())
        {
            row = rows.size();
            rows.append(new TableRow);
        }
        // if(row<0 or row>rows.size())
        return &(rows[row]->cells[col]);
    }

    void cutUp(int n)
    {
        for (int i = 0; i < n; i++)
        {
            rows.append(rows.takeFirst());
        }
    }
    void cutDown(int n)
    {
        for (int i = 0; i < n; i++)
        {
            TableRow *row = rows.takeLast();
            rows.prepend(row);
        }
    }
};

class VPointer : public QWidget
{
    Q_OBJECT
  public:
    VPointer(QWidget *parent);
    QPixmap *pix;
    QPixmap *pix_vcross;
    void setMovable(bool b) { flag_movable = b; }
    bool flag_movable;
  protected slots:
    //		void event_cursor_moved(QMouseEvent *e);
  protected:
    // virtual void drawButton 3( QPainter * ) ;
    virtual void paintEvent(QPaintEvent *event);
    //	void resizeEvent(QResizeEvent *p);
};

class FloatingHead : public QWidget
{
    Q_OBJECT
  public:
    FloatingHead(QWidget *parent);
    QPixmap *pix;
    void setTitle(QString str, int w, int h);
    QString title;
  protected slots:
  protected:
    virtual void paintEvent(QPaintEvent *event);
};

class TableHead : public QtTableView
{
    Q_OBJECT
  public:
    TableCache tablecache;
    TableHead(HeadedTable *parent = 0);
    virtual bool isCellChanged(int row, int col);
    virtual void checkProfile();

  protected slots:
    void scrollSideways(int);

  protected:
    virtual void paintCell(QPainter *p, int row, int col, bool update);
    virtual int cellWidth(int col);
    virtual void mousePressEvent(QMouseEvent *e);
    virtual void mouseReleaseEvent(QMouseEvent *e);
    virtual void mouseMoveEvent(QMouseEvent *e);
    virtual void eraseRight(QPainter *, QRect &r);

    virtual void dragEnterEvent(QDragEnterEvent *event);
    virtual void dragLeaveEvent(QDragLeaveEvent *event);

    FloatingHead *floatHead;

    HeadedTable *htable; // to access parent class
    QPoint press;
    int click_col; // physical column clicked in
    bool dragging;
    int drag_pos; // previous dragging position
    int drag_offset;
signals:
    void rightClicked(QPoint where, int col);
    void toolTip(QPoint where, int col);

    friend class HeadedTable;
};

class TableBody : public QtTableView
{
    Q_OBJECT
  public:
    TableCache tablecache;
    TableBody(HeadedTable *parent = 0);
    void drawGhostCol(int x, int w);
    // int zerowidth;		// width of the digit 0

    virtual bool isCellChanged(int row, int col);
    virtual void checkProfile();
  protected slots:
    // for keyboard accelerators
    void jumpTop();
    void jumpBottom();
    void centerVertically(int row);
    void showRange(int from, int to);

  protected:
    virtual void scrollTrigger(int x, int y); // tmp

    virtual void paintCell(QPainter *p, int row, int col, bool forced);
    virtual int cellWidth(int col);
    void mousePressEvent(QMouseEvent *e);
    void mouseDoubleClickEvent(QMouseEvent *e);
    void mouseMoveEvent(QMouseEvent *e);
    void mouseReleaseEvent(QMouseEvent *);
    void leaveEvent(QEvent *);

    void timerEvent(QTimerEvent *);
    void updateRow(int row);
    void dragSelectTo(int row);
    QColor getXorColor();
    HeadedTable *htable; // to access parent class
    int first_drag_row;  // row where drag started
    int prev_drag_row;   // row where drag was at last event
    bool autoscrolling;  // true if we are autoscrolling right now
    enum
    {
        UP,
        DOWN
    } scrolldir;
    static const int scroll_delay = 10; // time delay when autoscrolling, in ms
    bool gadget_click; // whether mouse press was on folding gadget
  private:
    QColor background;
    QColor color_text;
    friend class HeadedTable;
};

class HeadedTable : public QWidget
{
    Q_OBJECT
  public:
    HeadedTable(QWidget *parent, int opts = 0);
    ~HeadedTable();
    TableHead *header() { return head; } // interface

    TableHead *head;
    TableBody *body;

    CellAttribute *attr; // DEL temporarily buffer
    enum NodeState
    {
        Leaf,
        Open,
        Closed
    };

    void repaint_changed();
    int updateColWidth(int col);

    void resetWidths();
    void resetWidth(int col) { max_widths[col] = -1; } // dont use ?
    void setProxy(QWidget *w) { body->setFocusProxy(w); }
    void setSortedCol(int col);
    int sortedCol() { return sorted_col; }
    int numRows() { return nrows; }
    int numCols() { return ncols; }
    void setNumRows(int rows);
    void setNumCols(int cols);
    int clickedColumn() { return head->click_col; }
    void deleteCol(int col, bool update = true);
    int leftCell() { return body->leftCell(); };
    int lastColVisible() { return body->lastColVisible(); };
    int topCell() { return body->topCell(); };
    int lastRowVisible() { return body->lastRowVisible(); };
    void updateCell(int row, int col, bool erase = false);
    void updateHeading(int col){/* head->updateCell(0, col); */};
    void setAutoUpdate(bool update)
    {
        head->setAutoUpdate(update);
        body->setAutoUpdate(update);
    };
    void centerVertically(int row) { body->centerVertically(row); };
    void showRange(int from, int to) { body->showRange(from, to); };
    void repaintColumns(int col0, int col1 = -1);
    void setTreeMode(bool tm);
    bool treeMode() { return treemode; };
    int tableWidth() const
    {
        return body->totalWidth() + body->verticalScrollBar()->width();
    }

    void selectOnlyOne(int row);
    int numSelected() { return 0; }
    void clearAllSelections();

    virtual void setSelected(int row, bool sel){};
    virtual bool isSelected(int row) { return false; }
    virtual void checkTableModel(){};
    void fontChange();
signals:
    void titleClicked(int col);
    void selectionChanged();
    void doubleClicked(int row);
    void rightClicked(QPoint where);
    void foldSubTree(int row);
    void colMoved(int col, int place);
    void flyOnCell(int row, int col);
    void flyOnHCell(int col);
    void outOfCell();
    void outOfHCell();

  public slots:
    void selectAll();
    void repaintAll();

  protected:
    // These must be implemented in subclasses
    virtual QString title(int col) = 0;
    virtual QString text(int row, int col) = 0;
    virtual char *total_selectedRow(int col);
    // colWidth returns width in digit units; negative means variable width.
    virtual int colWidth(int col) = 0; // head_width
    virtual int alignment(int col) { return 0; }
    virtual int sizeHintForColumn(int col) const { return -1; }
    virtual void paintEvent(QPaintEvent *);
    virtual void hideEvent(QHideEvent *event);
    virtual void showEvent(QShowEvent *event);

    // may be reimplemented (default exists)
    virtual void moveCol(int col, int place);

    virtual QString tipText(int col);
    virtual int rowDepth(int row) { return 0; };
    virtual NodeState folded(int row) { return Leaf; };
    virtual int parentRow(int row) { return 0; };
    virtual bool lastChild(int row) { return false; };
    virtual bool columnMovable(int col) { return true; };
    // virtual bool modified(int row){return true;};
    virtual void overpaintCell(QPainter *p, int row, int col, int xpos){};

    void resizeEvent(QResizeEvent *);

    bool treemode;
    int treestep; // indentation for each tree level

  private:
    inline int computedWidth(int col);
    int colOffset(int col);
    inline int colXPos(int col);
    void updateCols(int deltacols, int place, bool update);

    int alignment_col[64];
    int headwidth_col[64];
    int max_widths[64]; // row widths, indexed by columns
    // Svec<int> widths;		// row widths, indexed by columns

    int sorted_col;    // column to emphasize
    int reversed_sort; // true if sorting backwards
    int options;
    int nrows;
    int ncols;

    // text cache is in logical columns (reorder causes no flush)
    bool folding;     // true if folding gadgets are used
    bool lines;       // true if tree lines are drawn
    int gadget_space; // horizontal space for show/hide tree gadget

    friend class TableHead;
    friend class TableBody;
};
#endif // HTABLE_H
