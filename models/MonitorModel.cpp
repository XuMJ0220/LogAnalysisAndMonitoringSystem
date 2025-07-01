#include "MonitorModel.h"
#include <QDebug>

namespace xumj {

MonitorModel::MonitorModel(QObject *parent) : QAbstractTableModel(parent) {}

void MonitorModel::setStatusList(const QJsonArray &statusList) {
    beginResetModel();
    statusArray.clear();
    for (int i = 0; i < statusList.size(); ++i) {
        statusArray.append(statusList.at(i).toObject());
    }
    endResetModel();
}

QJsonObject MonitorModel::getStatus(int row) const {
    if (row >= 0 && row < statusArray.size()) {
        return statusArray[row];
    }
    return QJsonObject();
}

int MonitorModel::columnCount(const QModelIndex &parent) const {
    Q_UNUSED(parent);
    return 4; // 模块名、状态、主要指标、备注
}

int MonitorModel::rowCount(const QModelIndex &parent) const {
    Q_UNUSED(parent);
    return statusArray.size();
}

QVariant MonitorModel::data(const QModelIndex &index, int role) const {
    if (!index.isValid() || index.row() >= statusArray.size())
        return QVariant();
    const QJsonObject &status = statusArray[index.row()];
    if (role == Qt::DisplayRole) {
        switch (index.column()) {
            case 0: return status.value("name").toString();
            case 1: return status.value("state").toString();
            case 2: return status.value("main_metric").toString();
            case 3: return status.value("remark").toString();
            default: return QVariant();
        }
    }
    return QVariant();
}

QVariant MonitorModel::headerData(int section, Qt::Orientation orientation, int role) const {
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole) {
        switch (section) {
            case 0: return "模块";
            case 1: return "状态";
            case 2: return "主要指标";
            case 3: return "备注";
            default: return QVariant();
        }
    }
    return QVariant();
}

} // namespace xumj 