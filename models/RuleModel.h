#pragma once
#include <QAbstractTableModel>
#include <QJsonArray>
#include <QJsonObject>
#include <QVector>

namespace xumj {

class RuleModel : public QAbstractTableModel {
    Q_OBJECT
public:
    explicit RuleModel(QObject *parent = nullptr);
    void setRules(const QJsonArray &rules);
    QJsonObject getRule(int row) const;
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
private:
    QVector<QJsonObject> ruleArray;
};

} // namespace xumj 