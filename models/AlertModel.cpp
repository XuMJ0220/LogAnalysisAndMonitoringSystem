#include "AlertModel.h"
#include <QColor>
#include <QDebug>

namespace xumj {

AlertModel::AlertModel(QObject *parent) : QAbstractTableModel(parent) {}

void AlertModel::setAlerts(const QJsonArray &alerts) {
    beginResetModel();
    alertArray.clear();
    for (int i = 0; i < alerts.size(); ++i) {
        alertArray.append(alerts.at(i).toObject());
    }
    endResetModel();
}

QJsonObject AlertModel::getAlert(int row) const {
    if (row >= 0 && row < alertArray.size()) {
        return alertArray[row];
    }
    return QJsonObject();
}

int AlertModel::columnCount(const QModelIndex &parent) const {
    Q_UNUSED(parent);
    return 6; // ID, 名称, 级别, 来源, 状态, 时间
}

int AlertModel::rowCount(const QModelIndex &parent) const {
    Q_UNUSED(parent);
    return alertArray.size();
}

QVariant AlertModel::data(const QModelIndex &index, int role) const {
    if (!index.isValid() || index.row() >= alertArray.size())
        return QVariant();
    
    const QJsonObject &alert = alertArray[index.row()];
    
    if (role == Qt::DisplayRole) {
        switch (index.column()) {
            case 0: return alert.value("id").toString();
            case 1: return alert.value("name").toString();
            case 2: return alert.value("level").toString();
            case 3: return alert.value("source").toString();
            case 4: return alert.value("status").toString();
            case 5: return formatTimestamp(alert.value("timestamp").toString());
            default: return QVariant();
        }
    } else if (role == Qt::BackgroundRole) {
        // 根据告警级别设置不同的背景色
        if (index.column() == 2) {
            return getAlertLevelColor(alert.value("level").toString());
        }
        // 根据告警状态设置不同的背景色
        if (index.column() == 4) {
            QString status = alert.value("status").toString();
            if (status == "RESOLVED")
                return QColor(200, 255, 200); // 浅绿色
            else if (status == "ACTIVE")
                return QColor(255, 200, 200); // 浅红色
            else if (status == "PENDING")
                return QColor(255, 255, 200); // 浅黄色
            else if (status == "IGNORED")
                return QColor(220, 220, 220); // 浅灰色
        }
    } else if (role == Qt::ToolTipRole) {
        // 显示告警描述作为提示
        return alert.value("description").toString();
    }
    
    return QVariant();
}

QVariant AlertModel::headerData(int section, Qt::Orientation orientation, int role) const {
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole) {
        switch (section) {
            case 0: return "ID";
            case 1: return "名称";
            case 2: return "级别";
            case 3: return "来源";
            case 4: return "状态";
            case 5: return "时间";
            default: return QVariant();
        }
    }
    return QVariant();
}

QString AlertModel::formatTimestamp(const QString &timestamp) const {
    QDateTime dateTime = QDateTime::fromString(timestamp, "yyyy-MM-dd HH:mm:ss");
    if (!dateTime.isValid()) {
        // 尝试其他可能的格式
        dateTime = QDateTime::fromString(timestamp, Qt::ISODate);
    }
    
    if (dateTime.isValid()) {
        return dateTime.toString("yyyy-MM-dd HH:mm:ss");
    }
    
    return timestamp; // 无法解析则返回原始字符串
}

QColor AlertModel::getAlertLevelColor(const QString &level) const {
    if (level == "CRITICAL")
        return QColor(255, 100, 100); // 深红色
    else if (level == "ERROR")
        return QColor(255, 150, 150); // 红色
    else if (level == "WARNING")
        return QColor(255, 255, 150); // 黄色
    else if (level == "INFO")
        return QColor(200, 200, 255); // 蓝色
    
    return QColor(255, 255, 255); // 白色（默认）
}

bool AlertModel::updateAlertStatus(const QString &alertId, const QString &newStatus) {
    for (int i = 0; i < alertArray.size(); ++i) {
        QJsonObject alert = alertArray[i];
        if (alert.value("id").toString() == alertId) {
            QJsonObject updatedAlert = alert;
            updatedAlert["status"] = newStatus;
            alertArray[i] = updatedAlert;
            
            QModelIndex idx = index(i, 4);
            emit dataChanged(idx, idx);
            return true;
        }
    }
    return false;
}

} // namespace xumj 