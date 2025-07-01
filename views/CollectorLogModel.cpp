#include "CollectorLogModel.h"
#include <QDateTime>
#include <QDebug>

namespace xumj {

CollectorLogModel::CollectorLogModel(QObject *parent)
    : QAbstractTableModel(parent) {}

void CollectorLogModel::setTotalCount(int total) {
    totalCount_ = total;
    emit layoutChanged();
}

void CollectorLogModel::setLogs(int offset, const QVector<CollectorLogEntry> &logs) {
    pageLogs_ = logs;
    emit dataChanged(index(0, 0), index(rowCount() - 1, columnCount() - 1));
}

CollectorLogEntry CollectorLogModel::getLog(int row) const {
    if (row >= 0 && row < pageLogs_.size()) return pageLogs_[row];
    return CollectorLogEntry();
}

int CollectorLogModel::rowCount(const QModelIndex &) const {
    return 20; // 每页固定20行
}

int CollectorLogModel::columnCount(const QModelIndex &) const {
    return 3;
}

QVariant CollectorLogModel::data(const QModelIndex &index, int role) const {
    if (!index.isValid() || index.row() >= pageLogs_.size()) return QVariant();
    const CollectorLogEntry &entry = pageLogs_[index.row()];
    if (role == Qt::DisplayRole) {
        switch (index.column()) {
            case 0: return entry.time;
            case 1: return entry.level;
            case 2: return entry.content;
            default: return QVariant();
        }
    }
    return QVariant();
}

QVariant CollectorLogModel::headerData(int section, Qt::Orientation orientation, int role) const {
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole) {
        switch (section) {
            case 0: return "时间";
            case 1: return "级别";
            case 2: return "内容";
            default: return QVariant();
        }
    }
    return QVariant();
}

void CollectorLogModel::addLog(const CollectorLogEntry &entry) {
    allLogs_.append(entry);
    totalCount_ = allLogs_.size();
    setPage(currentOffset_ / pageSize_);
    emit statsChanged(totalCount_);
}

void CollectorLogModel::clear() {
    allLogs_.clear();
    totalCount_ = 0;
    setPage(0);
    emit statsChanged(0);
}

bool CollectorLogModel::removeRows(int row, int count, const QModelIndex &parent) {
    int idx = currentOffset_ + row;
    if (idx < 0 || idx + count > allLogs_.size() || count <= 0) return false;
    beginRemoveRows(parent, row, row + count - 1);
    for (int i = 0; i < count; ++i) allLogs_.removeAt(idx);
    endRemoveRows();
    totalCount_ = allLogs_.size();
    setPage(currentOffset_ / pageSize_);
    emit statsChanged(totalCount_);
    return true;
}

int CollectorLogModel::logCount() const {
    return allLogs_.size();
}

void CollectorLogModel::setAllLogs(const QVector<CollectorLogEntry> &logs) {
    allLogs_ = logs;
    totalCount_ = allLogs_.size();
    setPage(0);
    emit statsChanged(totalCount_);
}

void CollectorLogModel::setPage(int page) {
    currentOffset_ = page * pageSize_;
    pageLogs_.clear();
    for (int i = 0; i < pageSize_; ++i) {
        int idx = currentOffset_ + i;
        if (idx < allLogs_.size())
            pageLogs_.append(allLogs_[idx]);
    }
    emit layoutChanged();
}

QVector<CollectorLogEntry> CollectorLogModel::getPageLogs() const {
    return pageLogs_;
}

int CollectorLogModel::totalCount() const {
    return totalCount_;
}

} // namespace xumj 