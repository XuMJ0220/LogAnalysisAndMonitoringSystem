#include "RuleModel.h"
#include <QDebug>

namespace xumj {

RuleModel::RuleModel(QObject *parent) : QAbstractTableModel(parent) {}

void RuleModel::setRules(const QJsonArray &rules) {
    beginResetModel();
    ruleArray.clear();
    for (int i = 0; i < rules.size(); ++i) {
        ruleArray.append(rules.at(i).toObject());
    }
    endResetModel();
}

QJsonObject RuleModel::getRule(int row) const {
    if (row >= 0 && row < ruleArray.size()) {
        return ruleArray[row];
    }
    return QJsonObject();
}

int RuleModel::columnCount(const QModelIndex &parent) const {
    Q_UNUSED(parent);
    return 4; // ID, 名称, 类型, 状态
}

int RuleModel::rowCount(const QModelIndex &parent) const {
    Q_UNUSED(parent);
    return ruleArray.size();
}

QVariant RuleModel::data(const QModelIndex &index, int role) const {
    if (!index.isValid() || index.row() >= ruleArray.size())
        return QVariant();
    const QJsonObject &rule = ruleArray[index.row()];
    if (role == Qt::DisplayRole) {
        switch (index.column()) {
            case 0: return rule.value("id").toString();
            case 1: return rule.value("name").toString();
            case 2: return rule.value("type").toString();
            case 3: return rule.value("enabled").toBool() ? "启用" : "禁用";
            default: return QVariant();
        }
    }
    return QVariant();
}

QVariant RuleModel::headerData(int section, Qt::Orientation orientation, int role) const {
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole) {
        switch (section) {
            case 0: return "ID";
            case 1: return "名称";
            case 2: return "类型";
            case 3: return "状态";
            default: return QVariant();
        }
    }
    return QVariant();
}

} // namespace xumj 