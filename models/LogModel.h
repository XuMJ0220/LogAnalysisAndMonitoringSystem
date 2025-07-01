#pragma once
#include <QAbstractTableModel>
#include <QJsonArray>
#include <QJsonObject>
#include <QDateTime>
#include <QVector>

namespace xumj {

class LogModel : public QAbstractTableModel {
    Q_OBJECT
public:
    explicit LogModel(QObject *parent = nullptr);
    void setTotalCount(int total);
    void setLogs(int offset, const QJsonArray &logs);
    QJsonObject getLog(int row) const;
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
private:
    QVector<QJsonObject> logArray;
    int totalCount_ = 0;
    QString formatTimestamp(qint64 timestamp) const;
};

} // namespace xumj 