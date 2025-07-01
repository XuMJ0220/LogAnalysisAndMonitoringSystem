#pragma once
#include <QAbstractTableModel>
#include <QVector>
#include <QString>

namespace xumj {

struct CollectorLogEntry {
    QString time;
    QString level;
    QString content;
};

class CollectorLogModel : public QAbstractTableModel {
    Q_OBJECT
public:
    explicit CollectorLogModel(QObject *parent = nullptr);
    void setTotalCount(int total);
    void setLogs(int offset, const QVector<CollectorLogEntry> &logs);
    CollectorLogEntry getLog(int row) const;
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

    void addLog(const CollectorLogEntry &entry);
    void clear();
    bool removeRows(int row, int count, const QModelIndex &parent = QModelIndex()) override;
    int logCount() const;

    void setAllLogs(const QVector<CollectorLogEntry> &logs);
    void setPage(int page);
    QVector<CollectorLogEntry> getPageLogs() const;
    int totalCount() const;

    const QVector<CollectorLogEntry>& allLogs() const { return allLogs_; }

signals:
    void statsChanged(int logCount);

private:
    QVector<CollectorLogEntry> pageLogs_; // 当前页日志
    QVector<CollectorLogEntry> allLogs_; // 全部日志
    int totalCount_ = 0;
    int currentOffset_ = 0; // 当前页起始下标
    int pageSize_ = 20; // 每页条数
};

} // namespace xumj 