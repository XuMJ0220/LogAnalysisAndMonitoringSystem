#pragma once
#include <QAbstractTableModel>
#include <QJsonArray>
#include <QJsonObject>
#include <QDateTime>
#include <QVector>

namespace xumj {

class AlertModel : public QAbstractTableModel {
    Q_OBJECT
public:
    explicit AlertModel(QObject *parent = nullptr);
    void setAlerts(const QJsonArray &alerts);
    QJsonObject getAlert(int row) const;
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
    
    // 更新告警状态
    bool updateAlertStatus(const QString &alertId, const QString &newStatus);
    
private:
    QVector<QJsonObject> alertArray;
    QString formatTimestamp(const QString &timestamp) const;
    QColor getAlertLevelColor(const QString &level) const;
};

} // namespace xumj 