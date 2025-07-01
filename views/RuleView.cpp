#include "RuleView.h"
#include <QVBoxLayout>
#include <QHeaderView>
#include <QMessageBox>
#include <QInputDialog>
#include <QJsonObject>
#include <QJsonDocument>

namespace xumj {

RuleView::RuleView(ApiClient *api, QWidget *parent) : QWidget(parent), apiClient(api) {
    setupUi();
    setupMaterialDesignStyle();
    setupConnections();
    selectedRuleId = QString();
}

void RuleView::setupUi() {
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(24, 24, 24, 24);
    mainLayout->setSpacing(16);

    // 工具栏
    QHBoxLayout *toolbarLayout = new QHBoxLayout();
    addBtn = new QPushButton("新增规则", this);
    editBtn = new QPushButton("编辑规则", this);
    deleteBtn = new QPushButton("删除规则", this);
    totalCountLabel = new QLabel("总规则数: 0", this);
    toolbarLayout->addWidget(addBtn);
    toolbarLayout->addWidget(editBtn);
    toolbarLayout->addWidget(deleteBtn);
    toolbarLayout->addStretch();
    toolbarLayout->addWidget(totalCountLabel);
    mainLayout->addLayout(toolbarLayout);

    // 表格
    table = new QTableView(this);
    model = new RuleModel(this);
    table->setModel(model);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setAlternatingRowColors(true);
    table->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    table->verticalHeader()->setDefaultSectionSize(40);
    table->verticalHeader()->setVisible(false);
    table->setColumnWidth(0, 120);
    table->setColumnWidth(1, 200);
    table->setColumnWidth(2, 120);
    table->setColumnWidth(3, 80);
    mainLayout->addWidget(table, 1);
}

void RuleView::setupMaterialDesignStyle() {
    setStyleSheet(R"(
        QPushButton {
            background-color: #2196F3;
            color: white;
            border: none;
            border-radius: 8px;
            padding: 8px 20px;
            font-weight: bold;
            font-size: 15px;
            min-height: 36px;
        }
        QPushButton:hover {
            background-color: #1976D2;
        }
        QPushButton:pressed {
            background-color: #0D47A1;
        }
        QPushButton:disabled {
            background-color: #BDBDBD;
            color: #757575;
        }
        QTableView {
            border: none;
            selection-background-color: #E3F2FD;
            selection-color: #212121;
        }
        QHeaderView::section {
            background-color: #F5F5F5;
            padding: 8px;
            border: none;
            border-bottom: 1px solid #E0E0E0;
            font-weight: bold;
        }
    )");
}

void RuleView::setupConnections() {
    connect(addBtn, &QPushButton::clicked, this, &RuleView::onAddRuleClicked);
    connect(editBtn, &QPushButton::clicked, this, &RuleView::onEditRuleClicked);
    connect(deleteBtn, &QPushButton::clicked, this, &RuleView::onDeleteRuleClicked);
    connect(table, &QTableView::clicked, this, &RuleView::onRuleTableClicked);
    connect(apiClient, &ApiClient::alertRulesReceived, this, &RuleView::onRulesReceived);
    connect(apiClient, &ApiClient::alertRuleCreated, this, &RuleView::onRuleOperationResult);
    connect(apiClient, &ApiClient::alertRuleUpdated, this, &RuleView::onRuleOperationResult);
    connect(apiClient, &ApiClient::alertRuleDeleted, this, &RuleView::onRuleOperationResult);
}

void RuleView::showEvent(QShowEvent *event) {
    QWidget::showEvent(event);
    loadRules();
}

void RuleView::loadRules() {
    apiClient->getAlertRules();
}

void RuleView::onRulesReceived(const QJsonArray &rules) {
    model->setRules(rules);
    totalCountLabel->setText(QString("总规则数: %1").arg(rules.size()));
    table->resizeColumnsToContents();
    if (rules.size() > 0) {
        QModelIndex firstIndex = model->index(0, 0);
        table->setCurrentIndex(firstIndex);
        onRuleTableClicked(firstIndex);
    }
}

void RuleView::onAddRuleClicked() {
    showRuleDialog();
}

void RuleView::onEditRuleClicked() {
    if (!selectedRuleId.isEmpty()) {
        int row = table->currentIndex().row();
        QJsonObject rule = model->getRule(row);
        showRuleDialog(rule);
    }
}

void RuleView::onDeleteRuleClicked() {
    if (!selectedRuleId.isEmpty()) {
        if (QMessageBox::question(this, "删除规则", "确定要删除该规则吗？") == QMessageBox::Yes) {
            apiClient->deleteAlertRule(selectedRuleId);
        }
    }
}

void RuleView::onRuleTableClicked(const QModelIndex &index) {
    if (index.isValid()) {
        int row = index.row();
        QJsonObject rule = model->getRule(row);
        selectedRuleId = rule.value("id").toString();
        editBtn->setEnabled(true);
        deleteBtn->setEnabled(true);
    } else {
        selectedRuleId.clear();
        editBtn->setEnabled(false);
        deleteBtn->setEnabled(false);
    }
}

void RuleView::showRuleDialog(const QJsonObject &rule) {
    // 简单实现：弹出输入框，仅支持编辑名称和类型
    bool ok = false;
    QString name = QInputDialog::getText(this, rule.isEmpty() ? "新增规则" : "编辑规则", "规则名称：", QLineEdit::Normal, rule.value("name").toString(), &ok);
    if (!ok || name.trimmed().isEmpty()) return;
    QString type = QInputDialog::getText(this, rule.isEmpty() ? "新增规则" : "编辑规则", "规则类型：", QLineEdit::Normal, rule.value("type").toString(), &ok);
    if (!ok || type.trimmed().isEmpty()) return;
    QJsonObject newRule = rule;
    newRule["name"] = name;
    newRule["type"] = type;
    newRule["enabled"] = true;
    if (rule.isEmpty()) {
        apiClient->createAlertRule(newRule);
    } else {
        apiClient->updateAlertRule(rule.value("id").toString(), newRule);
    }
}

void RuleView::onRuleOperationResult(bool success, const QString &message) {
    if (success) {
        loadRules();
    } else {
        QMessageBox::warning(this, "操作失败", message);
    }
}

} // namespace xumj 