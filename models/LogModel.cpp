#include "LogModel.h"
#include <QColor>
#include <QDebug>

namespace xumj {

LogModel::LogModel(QObject *parent) : QAbstractTableModel(parent) {}

void LogModel::setTotalCount(int total) {
    totalCount_ = total;
    logArray.resize(totalCount_);
    qDebug() << "setTotalCount: totalCount_=" << totalCount_ << ", logArray.size()=" << logArray.size();
    emit layoutChanged();
}

void LogModel::setLogs(int offset, const QJsonArray &logs) {
    qDebug() << "setLogs: offset=" << offset << ", logs.size()=" << logs.size();
    if (logArray.size() < offset + logs.size()) {
        logArray.resize(offset + logs.size());
    }
    for (int i = 0; i < logs.size(); ++i) {
        logArray[offset + i] = logs.at(i).toObject();
    }
    // 输出前10条logArray内容
    for (int i = 0; i < qMin(10, logArray.size()); ++i) {
        qDebug() << "logArray[" << i << "]=" << logArray[i];
    }
    // emit全表dataChanged，确保所有行都能刷新
    emit dataChanged(index(0, 0), index(rowCount() - 1, columnCount() - 1));
}

QJsonObject LogModel::getLog(int row) const {
    if (row >= 0 && row < logArray.size()) {
        return logArray[row];
    }
    return QJsonObject();
}

int LogModel::columnCount(const QModelIndex &parent) const {
    int count = 5;
    qDebug() << "columnCount called, return" << count;
    return count;
}

QVariant LogModel::data(const QModelIndex &index, int role) const {
    qDebug() << "data() called: row=" << index.row() << ", col=" << index.column() << ", valid=" << index.isValid() << ", role=" << role;
    if (!index.isValid() || index.row() >= logArray.size())
        return QVariant();
    const QJsonObject &log = logArray[index.row()];
    qDebug() << "log at row" << index.row() << ":" << log;
    if (role == Qt::DisplayRole) {
        if (log.isEmpty()) {
            if (index.column() == 0) return QString(); // 未加载时ID列为空
            return QString("加载中...");
        }
        switch (index.column()) {
            case 0: return log.value("log_id").toString();
            case 1: return formatTimestamp(log.value("timestamp").toVariant().toLongLong());
            case 2: return log.value("source").toString();
            case 3: return log.value("level").toString();
            case 4: return log.value("content").toString();
            default: return QVariant();
        }
    } else if (role == Qt::ToolTipRole && !log.isEmpty() && index.column() == 4) {
        return log.value("content").toString();
    } else if (role == Qt::BackgroundRole) {
        QString level = log.value("level").toString();
        if (level == "ERROR") {
            return QColor(255, 200, 200);
        } else if (level == "WARNING") {
            return QColor(255, 255, 200);
        }
    }
    return QVariant();
}

QVariant LogModel::headerData(int section, Qt::Orientation orientation, int role) const {
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole) {
        qDebug() << "headerData called, section=" << section;
        switch (section) {
            case 0: return "ID";
            case 1: return "时间";
            case 2: return "来源";
            case 3: return "级别";
            case 4: return "内容";
            default: return QVariant();
        }
    }
    return QVariant();
}

QString LogModel::formatTimestamp(qint64 timestamp) const {
    QDateTime dateTime = QDateTime::fromSecsSinceEpoch(timestamp);
    return dateTime.toString("yyyy-MM-dd HH:mm:ss");
}

int LogModel::rowCount(const QModelIndex &parent) const {
    Q_UNUSED(parent);
    // 每页固定显示20行，而不是显示所有数据
    int fixedPageSize = 20;
    qDebug() << "rowCount called, return" << fixedPageSize;
    return fixedPageSize;
}

} // namespace xumj 