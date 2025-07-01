#include "AlertView.h"
#include <QVBoxLayout>
#include <QHeaderView>
#include <QScrollBar>
#include <QTimer>
#include <QUrlQuery>
#include <QDebug>
#include <QApplication>
#include <QSet>
#include <QShowEvent>
#include <QPushButton>
#include <QFrame>
#include <QGraphicsDropShadowEffect>
#include <QTableView>
#include <QMessageBox>

namespace xumj {

AlertView::AlertView(ApiClient *api, QWidget *parent) : QWidget(parent), apiClient(api) {
    setupUi();
    setupMaterialDesignStyle();
    setupConnections();
    selectedAlertId = QString();
}

void AlertView::setupUi() {
    // 创建主布局
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(24, 24, 24, 24);
    mainLayout->setSpacing(16);
    
    // 工具栏卡片
    createToolbarCard(mainLayout);
    // 过滤卡片
    createFilterCard(mainLayout);
    
    // 创建表格和详情的水平布局
    QHBoxLayout *contentLayout = new QHBoxLayout();
    contentLayout->setSpacing(16);
    
    // 创建表格布局（占左侧2/3）
    QVBoxLayout *tableLayout = new QVBoxLayout();
    createTableCard(tableLayout);
    contentLayout->addLayout(tableLayout, 2);
    
    // 创建详情布局（占右侧1/3）
    QVBoxLayout *detailLayout = new QVBoxLayout();
    createDetailCard(detailLayout);
    contentLayout->addLayout(detailLayout, 1);
    
    // 添加内容布局到主布局
    mainLayout->addLayout(contentLayout, 1);
}

void AlertView::createToolbarCard(QVBoxLayout *mainLayout) {
    QFrame *toolbarCard = new QFrame(this);
    toolbarCard->setObjectName("toolbarCard");
    
    QHBoxLayout *toolbarLayout = new QHBoxLayout(toolbarCard);
    toolbarLayout->setContentsMargins(20, 16, 20, 16);
    
    // 刷新按钮
    refreshBtn = new QPushButton("刷新告警", toolbarCard);
    refreshBtn->setObjectName("refreshButton");
    refreshBtn->setIcon(QIcon::fromTheme("view-refresh"));
    
    // 总数标签
    totalCountLabel = new QLabel("总告警数: 0", toolbarCard);
    totalCountLabel->setObjectName("totalCountLabel");
    
    toolbarLayout->addWidget(refreshBtn);
    toolbarLayout->addStretch();
    toolbarLayout->addWidget(totalCountLabel);
    
    mainLayout->addWidget(toolbarCard);
}

void AlertView::createFilterCard(QVBoxLayout *mainLayout) {
    QFrame *filterCard = new QFrame(this);
    filterCard->setObjectName("filterCard");
    
    QHBoxLayout *filterLayout = new QHBoxLayout(filterCard);
    filterLayout->setContentsMargins(20, 16, 20, 16);
    filterLayout->setSpacing(16);
    
    // 级别过滤
    QLabel *levelLabel = new QLabel("级别:", filterCard);
    levelLabel->setObjectName("filterLabel");
    levelFilter = new QComboBox(filterCard);
    levelFilter->setObjectName("levelFilter");
    levelFilter->addItem("全部级别", "");
    levelFilter->addItem("INFO", "INFO");
    levelFilter->addItem("WARNING", "WARNING");
    levelFilter->addItem("ERROR", "ERROR");
    levelFilter->addItem("CRITICAL", "CRITICAL");
    levelFilter->setMinimumHeight(40);
    
    // 来源过滤
    QLabel *sourceLabel = new QLabel("来源:", filterCard);
    sourceLabel->setObjectName("filterLabel");
    sourceFilter = new QComboBox(filterCard);
    sourceFilter->setObjectName("sourceFilter");
    sourceFilter->addItem("全部来源", "");
    sourceFilter->addItem("system", "system");
    sourceFilter->addItem("application", "application");
    sourceFilter->addItem("database", "database");
    sourceFilter->addItem("api-gateway", "api-gateway");
    sourceFilter->addItem("security", "security");
    sourceFilter->setMinimumHeight(40);
    
    // 状态过滤
    QLabel *statusLabel = new QLabel("状态:", filterCard);
    statusLabel->setObjectName("filterLabel");
    statusFilter = new QComboBox(filterCard);
    statusFilter->setObjectName("statusFilter");
    statusFilter->addItem("全部状态", "");
    statusFilter->addItem("待处理", "PENDING");
    statusFilter->addItem("活跃", "ACTIVE");
    statusFilter->addItem("已解决", "RESOLVED");
    statusFilter->addItem("已忽略", "IGNORED");
    statusFilter->setMinimumHeight(40);
    
    filterLayout->addWidget(levelLabel);
    filterLayout->addWidget(levelFilter);
    filterLayout->addWidget(sourceLabel);
    filterLayout->addWidget(sourceFilter);
    filterLayout->addWidget(statusLabel);
    filterLayout->addWidget(statusFilter);
    filterLayout->addStretch();
    
    mainLayout->addWidget(filterCard);
}

void AlertView::createTableCard(QVBoxLayout *mainLayout) {
    QFrame *tableCard = new QFrame(this);
    tableCard->setObjectName("tableCard");
    QVBoxLayout *tableLayout = new QVBoxLayout(tableCard);
    tableLayout->setContentsMargins(0, 0, 0, 0);
    
    // 创建表格视图
    table = new QTableView(tableCard);
    table->setObjectName("alertTable");
    model = new AlertModel(tableCard);
    table->setModel(model);
    
    // 设置表格属性
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setAlternatingRowColors(true);
    table->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    table->verticalHeader()->setDefaultSectionSize(48);
    table->verticalHeader()->setVisible(true);
    
    // 启用鼠标滚轮滚动功能
    table->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    table->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    
    // 设置每列初始宽度
    table->setColumnWidth(0, 100); // ID
    table->setColumnWidth(1, 200); // 名称
    table->setColumnWidth(2, 80);  // 级别
    table->setColumnWidth(3, 120); // 来源
    table->setColumnWidth(4, 80);  // 状态
    table->setColumnWidth(5, 160); // 时间
    
    tableLayout->addWidget(table);
    mainLayout->addWidget(tableCard, 1); // 拉伸因子1
}

void AlertView::createDetailCard(QVBoxLayout *mainLayout) {
    QFrame *detailCard = new QFrame(this);
    detailCard->setObjectName("detailCard");
    QVBoxLayout *detailLayout = new QVBoxLayout(detailCard);
    detailLayout->setContentsMargins(20, 20, 20, 20);
    detailLayout->setSpacing(16);
    
    // 详情标题
    detailTitleLabel = new QLabel("告警详情", detailCard);
    detailTitleLabel->setObjectName("detailTitleLabel");
    detailTitleLabel->setAlignment(Qt::AlignCenter);
    detailTitleLabel->setWordWrap(true);
    
    // 基本信息组
    QFrame *infoFrame = new QFrame(detailCard);
    infoFrame->setObjectName("infoFrame");
    QGridLayout *infoLayout = new QGridLayout(infoFrame);
    infoLayout->setContentsMargins(10, 10, 10, 10);
    infoLayout->setSpacing(8);
    
    // 来源
    infoLayout->addWidget(new QLabel("来源:", infoFrame), 0, 0);
    detailSourceLabel = new QLabel("-", infoFrame);
    detailSourceLabel->setObjectName("detailInfoLabel");
    infoLayout->addWidget(detailSourceLabel, 0, 1);
    
    // 级别
    infoLayout->addWidget(new QLabel("级别:", infoFrame), 1, 0);
    detailLevelLabel = new QLabel("-", infoFrame);
    detailLevelLabel->setObjectName("detailInfoLabel");
    infoLayout->addWidget(detailLevelLabel, 1, 1);
    
    // 状态
    infoLayout->addWidget(new QLabel("状态:", infoFrame), 2, 0);
    detailStatusLabel = new QLabel("-", infoFrame);
    detailStatusLabel->setObjectName("detailInfoLabel");
    infoLayout->addWidget(detailStatusLabel, 2, 1);
    
    // 时间
    infoLayout->addWidget(new QLabel("时间:", infoFrame), 3, 0);
    detailTimeLabel = new QLabel("-", infoFrame);
    detailTimeLabel->setObjectName("detailInfoLabel");
    infoLayout->addWidget(detailTimeLabel, 3, 1);
    
    // 描述
    QLabel *descLabel = new QLabel("描述:", detailCard);
    descLabel->setObjectName("detailSectionLabel");
    detailDescriptionLabel = new QLabel("-", detailCard);
    detailDescriptionLabel->setObjectName("detailDescriptionLabel");
    detailDescriptionLabel->setWordWrap(true);
    detailDescriptionLabel->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    detailDescriptionLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    
    // 操作按钮
    QHBoxLayout *actionLayout = new QHBoxLayout();
    
    resolveBtn = new QPushButton("解决告警", detailCard);
    resolveBtn->setObjectName("resolveButton");
    resolveBtn->setEnabled(false);
    
    ignoreBtn = new QPushButton("忽略告警", detailCard);
    ignoreBtn->setObjectName("ignoreButton");
    ignoreBtn->setEnabled(false);
    
    actionLayout->addWidget(resolveBtn);
    actionLayout->addWidget(ignoreBtn);
    
    // 添加所有组件到详情布局
    detailLayout->addWidget(detailTitleLabel);
    detailLayout->addWidget(infoFrame);
    detailLayout->addWidget(descLabel);
    detailLayout->addWidget(detailDescriptionLabel, 1); // 拉伸因子1
    detailLayout->addLayout(actionLayout);
    
    mainLayout->addWidget(detailCard);
}

void AlertView::setupMaterialDesignStyle() {
    setStyleSheet(R"(
        QFrame#toolbarCard, QFrame#filterCard, QFrame#tableCard, QFrame#detailCard, QFrame#infoFrame {
            background-color: white;
            border-radius: 16px;
            border: 1.5px solid #E0E0E0;
            box-shadow: 0 2px 8px rgba(33,150,243,0.08);
        }
        
        QFrame#infoFrame {
            background-color: #F5F5F5;
            border-radius: 8px;
        }
        
        QPushButton#refreshButton, QPushButton#resolveButton, QPushButton#ignoreButton {
            background-color: #2196F3;
            color: white;
            border: none;
            border-radius: 8px;
            padding: 10px 24px;
            font-weight: bold;
            font-size: 16px;
            min-height: 40px;
        }
        
        QPushButton#resolveButton {
            background-color: #4CAF50;
        }
        
        QPushButton#ignoreButton {
            background-color: #FF9800;
        }
        
        QPushButton:hover {
            background-color: #1976D2;
        }
        
        QPushButton#resolveButton:hover {
            background-color: #388E3C;
        }
        
        QPushButton#ignoreButton:hover {
            background-color: #F57C00;
        }
        
        QPushButton:pressed {
            background-color: #0D47A1;
        }
        
        QPushButton:disabled {
            background-color: #BDBDBD;
            color: #757575;
        }
        
        QLabel#detailTitleLabel {
            font-size: 20px;
            font-weight: bold;
            color: #2196F3;
        }
        
        QLabel#detailSectionLabel {
            font-size: 16px;
            font-weight: bold;
            color: #424242;
        }
        
        QLabel#detailInfoLabel {
            font-size: 14px;
            color: #212121;
        }
        
        QLabel#detailDescriptionLabel {
            font-size: 14px;
            color: #212121;
            background-color: #F5F5F5;
            border-radius: 8px;
            padding: 10px;
        }
        
        QComboBox {
            border: 2px solid #BDBDBD;
            border-radius: 8px;
            padding: 8px 12px;
            font-size: 14px;
        }
        
        QComboBox:focus {
            border-color: #2196F3;
        }
        
        QComboBox::drop-down {
            subcontrol-origin: padding;
            subcontrol-position: center right;
            width: 24px;
            border-left-width: 0px;
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

void AlertView::setupConnections() {
    connect(refreshBtn, &QPushButton::clicked, this, &AlertView::onRefreshClicked);
    connect(resolveBtn, &QPushButton::clicked, this, &AlertView::onResolveClicked);
    connect(ignoreBtn, &QPushButton::clicked, this, &AlertView::onIgnoreClicked);
    
    connect(levelFilter, QOverload<int>::of(&QComboBox::currentIndexChanged), 
            this, &AlertView::onFilterChanged);
    connect(sourceFilter, QOverload<int>::of(&QComboBox::currentIndexChanged), 
            this, &AlertView::onFilterChanged);
    connect(statusFilter, QOverload<int>::of(&QComboBox::currentIndexChanged), 
            this, &AlertView::onFilterChanged);
    
    connect(table, &QTableView::clicked, this, &AlertView::onAlertTableClicked);
    
    connect(apiClient, &ApiClient::alertsReceived, this, &AlertView::onAlertsReceived);
    connect(apiClient, &ApiClient::alertStatusUpdated, this, &AlertView::onAlertStatusUpdated);
}

void AlertView::showEvent(QShowEvent *event) {
    QWidget::showEvent(event);
    // 当视图显示时加载告警数据
    loadAlerts();
}

void AlertView::loadAlerts() {
    QUrlQuery query;
    
    // 添加过滤条件
    if (!levelFilter->currentData().toString().isEmpty()) {
        query.addQueryItem("level", levelFilter->currentData().toString());
    }
    
    if (!sourceFilter->currentData().toString().isEmpty()) {
        query.addQueryItem("source", sourceFilter->currentData().toString());
    }
    
    if (!statusFilter->currentData().toString().isEmpty()) {
        query.addQueryItem("status", statusFilter->currentData().toString());
    }
    
    apiClient->getAlerts(query);
}

void AlertView::onAlertsReceived(const QJsonArray &alerts) {
    model->setAlerts(alerts);
    totalCountLabel->setText(QString("总告警数: %1").arg(alerts.size()));
    
    // 更新表格列宽
    table->resizeColumnsToContents();
    
    // 如果没有选中的告警，清空详情面板
    if (selectedAlertId.isEmpty() && alerts.size() > 0) {
        // 自动选择第一条告警
        QModelIndex firstIndex = model->index(0, 0);
        table->setCurrentIndex(firstIndex);
        onAlertTableClicked(firstIndex);
    } else if (alerts.size() == 0) {
        // 如果没有告警，禁用按钮
        resolveBtn->setEnabled(false);
        ignoreBtn->setEnabled(false);
        
        // 清空详情
        detailTitleLabel->setText("告警详情");
        detailSourceLabel->setText("-");
        detailLevelLabel->setText("-");
        detailStatusLabel->setText("-");
        detailTimeLabel->setText("-");
        detailDescriptionLabel->setText("-");
    }
}

void AlertView::onFilterChanged() {
    loadAlerts();
}

void AlertView::onRefreshClicked() {
    loadAlerts();
}

void AlertView::onResolveClicked() {
    if (!selectedAlertId.isEmpty()) {
        apiClient->updateAlertStatus(selectedAlertId, "RESOLVED");
    }
}

void AlertView::onIgnoreClicked() {
    if (!selectedAlertId.isEmpty()) {
        apiClient->updateAlertStatus(selectedAlertId, "IGNORED");
    }
}

void AlertView::onAlertTableClicked(const QModelIndex &index) {
    if (index.isValid()) {
        int row = index.row();
        QJsonObject alert = model->getAlert(row);
        showAlertDetail(alert);
        
        // 保存当前选中的告警ID
        selectedAlertId = alert.value("id").toString();
        
        // 根据告警状态启用/禁用按钮
        QString status = alert.value("status").toString();
        bool enableButtons = (status == "PENDING" || status == "ACTIVE");
        resolveBtn->setEnabled(enableButtons);
        ignoreBtn->setEnabled(enableButtons);
    }
}

void AlertView::showAlertDetail(const QJsonObject &alert) {
    if (alert.isEmpty()) {
        return;
    }
    
    detailTitleLabel->setText(alert.value("name").toString());
    detailSourceLabel->setText(alert.value("source").toString());
    detailLevelLabel->setText(alert.value("level").toString());
    detailStatusLabel->setText(alert.value("status").toString());
    detailTimeLabel->setText(alert.value("timestamp").toString());
    detailDescriptionLabel->setText(alert.value("description").toString());
}

void AlertView::onAlertStatusUpdated(const QString &alertId, bool success) {
    if (success) {
        // 更新模型中的告警状态
        if (alertId == selectedAlertId) {
            // 重新加载告警列表
            loadAlerts();
        }
    } else {
        QMessageBox::warning(this, "更新状态失败", "无法更新告警状态，请稍后再试。");
    }
}

} // namespace xumj 