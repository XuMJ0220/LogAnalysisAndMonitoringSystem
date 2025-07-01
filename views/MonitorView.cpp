#include "MonitorView.h"
#include <QVBoxLayout>
#include <QHeaderView>
#include <QTimer>

namespace xumj {

MonitorView::MonitorView(ApiClient *api, QWidget *parent) : QWidget(parent), apiClient(api) {
    setupUi();
    setupMaterialDesignStyle();
    setupConnections();
    autoRefreshTimer = new QTimer(this);
    autoRefreshTimer->setInterval(10000); // 10秒自动刷新
}

void MonitorView::setupUi() {
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(24, 24, 24, 24);
    mainLayout->setSpacing(16);

    // 工具栏
    QHBoxLayout *toolbarLayout = new QHBoxLayout();
    refreshBtn = new QPushButton("刷新", this);
    totalCountLabel = new QLabel("模块数: 0", this);
    toolbarLayout->addWidget(refreshBtn);
    toolbarLayout->addStretch();
    toolbarLayout->addWidget(totalCountLabel);
    mainLayout->addLayout(toolbarLayout);

    // 表格
    table = new QTableView(this);
    model = new MonitorModel(this);
    table->setModel(model);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setAlternatingRowColors(true);
    table->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    table->verticalHeader()->setDefaultSectionSize(40);
    table->verticalHeader()->setVisible(false);
    table->setColumnWidth(0, 160);
    table->setColumnWidth(1, 100);
    table->setColumnWidth(2, 200);
    table->setColumnWidth(3, 200);
    mainLayout->addWidget(table, 1);
}

void MonitorView::setupMaterialDesignStyle() {
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

void MonitorView::setupConnections() {
    connect(refreshBtn, &QPushButton::clicked, this, &MonitorView::onRefreshClicked);
    connect(apiClient, &ApiClient::systemStatusReceived, this, &MonitorView::onSystemStatusReceived);
    connect(autoRefreshTimer, &QTimer::timeout, this, &MonitorView::onAutoRefreshTimeout);
}

void MonitorView::showEvent(QShowEvent *event) {
    QWidget::showEvent(event);
    loadStatus();
    autoRefreshTimer->start();
}

void MonitorView::loadStatus() {
    apiClient->getSystemStatus();
}

void MonitorView::onSystemStatusReceived(const QJsonObject &status) {
    // 假设status["modules"]为QJsonArray
    QJsonArray modules = status.value("modules").toArray();
    model->setStatusList(modules);
    totalCountLabel->setText(QString("模块数: %1").arg(modules.size()));
    table->resizeColumnsToContents();
}

void MonitorView::onRefreshClicked() {
    loadStatus();
}

void MonitorView::onAutoRefreshTimeout() {
    loadStatus();
}

} // namespace xumj 