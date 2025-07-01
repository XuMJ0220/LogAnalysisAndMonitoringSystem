#pragma once
#include <QAbstractTableModel>
#include <QJsonArray>
#include <QJsonObject>
#include <QVector>

namespace xumj {

class MonitorModel : public QAbstractTableModel {
    Q_OBJECT
public:
    explicit MonitorModel(QObject *parent = nullptr);
    void setStatusList(const QJsonArray &statusList);
    QJsonObject getStatus(int row) const;
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
private:
    QVector<QJsonObject> statusArray;
};

} // namespace xumj 