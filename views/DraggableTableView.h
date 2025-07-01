#pragma once
#include <QTableView>

class DraggableTableView : public QTableView
{
    Q_OBJECT
public:
    explicit DraggableTableView(QWidget *parent = nullptr);
protected:
    void mouseMoveEvent(QMouseEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void leaveEvent(QEvent *event) override;
private:
    int hoverSection = -1;
    bool dragging = false;
    int dragSection = -1;
    int dragStartX = 0;
    int dragStartWidth = 0;
    int sectionAtPos(int x) const;
    int sectionEdgeAtPos(int x, int margin = 4) const;
}; 