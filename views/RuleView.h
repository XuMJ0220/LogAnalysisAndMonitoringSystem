#pragma once
#include <QWidget>
#include <QPushButton>
#include <QLineEdit>
#include <QComboBox>
#include <QLabel>
#include <QHBoxLayout>
#include <QTableView>
#include "../models/RuleModel.h"
#include "../ApiClient.h"

namespace xumj {

class RuleView : public QWidget {
    Q_OBJECT
public:
    RuleView(ApiClient *api, QWidget *parent = nullptr);
protected:
    void showEvent(QShowEvent *event) override;
private slots:
    void onRulesReceived(const QJsonArray &rules);
    void onAddRuleClicked();
    void onEditRuleClicked();
    void onDeleteRuleClicked();
    void onRuleTableClicked(const QModelIndex &index);
    void onRuleOperationResult(bool success, const QString &message);
private:
    void setupUi();
    void setupConnections();
    void setupMaterialDesignStyle();
    void loadRules();
    void showRuleDialog(const QJsonObject &rule = QJsonObject());

    QTableView *table;
    RuleModel *model;
    ApiClient *apiClient;
    QPushButton *addBtn;
    QPushButton *editBtn;
    QPushButton *deleteBtn;
    QLabel *totalCountLabel;
    QString selectedRuleId;
};

} // namespace xumj 