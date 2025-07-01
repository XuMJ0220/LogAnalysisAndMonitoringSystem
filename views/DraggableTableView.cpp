#include "DraggableTableView.h"
#include <QHeaderView>
#include <QMouseEvent>
#include <QCursor>

DraggableTableView::DraggableTableView(QWidget *parent)
    : QTableView(parent) {
    setMouseTracking(true);
}

int DraggableTableView::sectionAtPos(int x) const {
    int xpos = 0;
    for (int i = 0; i < model()->columnCount(); ++i) {
        xpos += columnWidth(i);
        if (x < xpos)
            return i;
    }
    return -1;
}

int DraggableTableView::sectionEdgeAtPos(int x, int margin) const {
    // 检查最左侧（第一列和表格左边界之间）
    if (x <= margin) return 0;
    int xpos = 0;
    int colCount = model()->columnCount();
    for (int i = 0; i < colCount - 1; ++i) {
        xpos += columnWidth(i);
        if (abs(x - xpos) <= margin)
            return i;
    }
    // 检查最右侧（最后一列和表格右边界之间）
    xpos += columnWidth(colCount - 1);
    if (abs(x - xpos) <= margin) {
        return colCount - 1;
    }
    return -1;
}

void DraggableTableView::mouseMoveEvent(QMouseEvent *event) {
    if (dragging && dragSection >= 0) {
        int delta = event->x() - dragStartX;
        int newWidth = dragStartWidth + delta;
        if (newWidth > 20) {
            setColumnWidth(dragSection, newWidth);
        }
        return;
    }
    int edge = sectionEdgeAtPos(event->x());
    if (edge >= 0) {
        setCursor(Qt::SplitHCursor);
        hoverSection = edge;
    } else {
        setCursor(Qt::ArrowCursor);
        hoverSection = -1;
    }
    QTableView::mouseMoveEvent(event);
}

void DraggableTableView::mousePressEvent(QMouseEvent *event) {
    int edge = sectionEdgeAtPos(event->x());
    if (edge >= 0 && event->button() == Qt::LeftButton) {
        dragging = true;
        dragSection = edge;
        dragStartX = event->x();
        dragStartWidth = columnWidth(edge);
        setCursor(Qt::SplitHCursor);
        return;
    }
    QTableView::mousePressEvent(event);
}

void DraggableTableView::mouseReleaseEvent(QMouseEvent *event) {
    if (dragging && event->button() == Qt::LeftButton) {
        dragging = false;
        dragSection = -1;
        setCursor(Qt::ArrowCursor);
        return;
    }
    QTableView::mouseReleaseEvent(event);
}

void DraggableTableView::leaveEvent(QEvent *event) {
    setCursor(Qt::ArrowCursor);
    hoverSection = -1;
    QTableView::leaveEvent(event);
} 