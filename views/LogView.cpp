#include "LogView.h"
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
#include "DraggableTableView.h"

namespace xumj {

LogView::LogView(ApiClient *api, QWidget *parent) : QWidget(parent), apiClient(api), currentOffset(0), pageSize(20), isLoading(false) {
    // 先创建UI
    setupUi();
    // 再设置样式
    setupMaterialDesignStyle();
    // 然后连接信号
    setupConnections();
    // 最后初始化数据
    requestedOffsets.clear();
    currentOffset = 0;
    applyFilters(true);
    setupScrollConnection();
    QTimer::singleShot(0, this, [this]() {
        setupScrollConnection();
        qDebug() << "[INIT] QTimer触发再次setupScrollConnection，确保滚动条信号连接";
    });
    qDebug() << "LogView构造后, model addr=" << model << ", table->model()=" << table->model();
    table->setModel(model);
    table->resizeColumnsToContents();
    table->show();
    qDebug() << "LogView构造完成, model=" << model << ", rowCount=" << model->rowCount();
}

void LogView::setupUi() {
    // 创建主布局
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(24, 24, 24, 24);
    mainLayout->setSpacing(16);
    
    // 工具栏卡片
    createToolbarCard(mainLayout);
    // 过滤卡片
    createFilterCard(mainLayout);
    // 表格卡片（拉伸因子1，自动填满空间）
    createTableCard(mainLayout); // 只加表格卡片
    // 分页卡片（始终在表格下方）
    createPaginationCard(mainLayout); // 直接加到主布局
}

void LogView::createToolbarCard(QVBoxLayout *mainLayout) {
    QFrame *toolbarCard = new QFrame(this);
    toolbarCard->setObjectName("toolbarCard");
    
    QHBoxLayout *toolbarLayout = new QHBoxLayout(toolbarCard);
    toolbarLayout->setContentsMargins(20, 16, 20, 16);
    
    // 刷新按钮
    refreshBtn = new QPushButton("刷新日志", toolbarCard);
    refreshBtn->setObjectName("refreshButton");
    refreshBtn->setIcon(QIcon::fromTheme("view-refresh"));
    
    // 总数标签
    totalCountLabel = new QLabel("总日志数: 0", toolbarCard);
    totalCountLabel->setObjectName("totalCountLabel");
    
    toolbarLayout->addWidget(refreshBtn);
    toolbarLayout->addStretch();
    toolbarLayout->addWidget(totalCountLabel);
    
    mainLayout->addWidget(toolbarCard);
}

void LogView::createFilterCard(QVBoxLayout *mainLayout) {
    QFrame *filterCard = new QFrame(this);
    filterCard->setObjectName("filterCard");
    
    QVBoxLayout *filterLayout = new QVBoxLayout(filterCard);
    filterLayout->setContentsMargins(20, 20, 20, 20);
    filterLayout->setSpacing(16);
    
    // 搜索框
    searchEdit = new QLineEdit(filterCard);
    searchEdit->setObjectName("searchEdit");
    searchEdit->setPlaceholderText("搜索日志内容...");
    searchEdit->setMinimumHeight(48);
    
    // 过滤控件行
    QHBoxLayout *filterRowLayout = new QHBoxLayout();
    filterRowLayout->setSpacing(16);
    
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
    sourceFilter->addItem("network", "network");
    sourceFilter->addItem("user-service", "user-service");
    sourceFilter->addItem("payment-service", "payment-service");
    sourceFilter->addItem("cache-service", "cache-service");
    sourceFilter->addItem("task-scheduler", "task-scheduler");
    sourceFilter->setMinimumHeight(40);
    
    filterRowLayout->addWidget(levelLabel);
    filterRowLayout->addWidget(levelFilter);
    filterRowLayout->addWidget(sourceLabel);
    filterRowLayout->addWidget(sourceFilter);
    filterRowLayout->addStretch();
    
    filterLayout->addWidget(searchEdit);
    filterLayout->addLayout(filterRowLayout);
    
    mainLayout->addWidget(filterCard);
}

void LogView::createTableCard(QVBoxLayout *mainLayout) {
    QFrame *tableCard = new QFrame(this);
    tableCard->setObjectName("tableCard");
    QVBoxLayout *tableLayout = new QVBoxLayout(tableCard);
    tableLayout->setContentsMargins(0, 0, 0, 0);
    // 创建表格视图
    table = new DraggableTableView(tableCard);
    table->setObjectName("logTable");
    model = new LogModel(tableCard);
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
    table->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    table->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    
    // 设置每列初始宽度
    table->setColumnWidth(0, 240); // ID
    table->setColumnWidth(1, 160); // 时间
    table->setColumnWidth(2, 120); // 来源
    table->setColumnWidth(3, 100); // 级别
    table->setColumnWidth(4, 500); // 内容
    
    tableLayout->addWidget(table);
    mainLayout->addWidget(tableCard, 1); // 拉伸因子1
}

void LogView::createPaginationCard(QVBoxLayout *mainLayout) {
    QFrame *paginationCard = new QFrame(this);
    paginationCard->setObjectName("paginationCard");
    QHBoxLayout *paginationLayout = new QHBoxLayout(paginationCard);
    paginationLayout->setContentsMargins(20, 16, 20, 16);
    
    // 分页控件
    prevPageBtn = new QPushButton("上一页", paginationCard);
    prevPageBtn->setObjectName("pageButton");
    prevPageBtn->setIcon(QIcon::fromTheme("go-previous"));
    
    pageInfoLabel = new QLabel(paginationCard);
    pageInfoLabel->setObjectName("pageInfoLabel");
    
    nextPageBtn = new QPushButton("下一页", paginationCard);
    nextPageBtn->setObjectName("pageButton");
    nextPageBtn->setIcon(QIcon::fromTheme("go-next"));
    
    // 跳转控件
    QLabel *jumpLabel = new QLabel("跳转到", paginationCard);
    jumpLabel->setObjectName("jumpLabel");
    
    jumpPageEdit = new QLineEdit(paginationCard);
    jumpPageEdit->setObjectName("jumpPageEdit");
    jumpPageEdit->setFixedWidth(60);
    jumpPageEdit->setPlaceholderText("页码");
    jumpPageEdit->setAlignment(Qt::AlignCenter);
    
    jumpPageBtn = new QPushButton("跳转", paginationCard);
    jumpPageBtn->setObjectName("jumpButton");
    
    paginationLayout->addWidget(prevPageBtn);
    paginationLayout->addWidget(pageInfoLabel);
    paginationLayout->addWidget(nextPageBtn);
    paginationLayout->addStretch();
    paginationLayout->addWidget(jumpLabel);
    paginationLayout->addWidget(jumpPageEdit);
    paginationLayout->addWidget(jumpPageBtn);
    
    mainLayout->addWidget(paginationCard); // 直接加到主布局
}

void LogView::setupMaterialDesignStyle() {
    setStyleSheet(R"(
        QFrame#toolbarCard, QFrame#filterCard, QFrame#tableCard, QFrame#paginationCard {
            background-color: white;
            border-radius: 16px;
            border: 1.5px solid #E0E0E0;
            box-shadow: 0 2px 8px rgba(33,150,243,0.08);
        }
        QPushButton#refreshButton, QPushButton#pageButton, QPushButton#jumpButton {
            background-color: #2196F3;
            color: white;
            border: none;
            border-radius: 8px;
            padding: 10px 24px;
            font-weight: bold;
            font-size: 16px;
            min-height: 40px;
            transition: background 0.2s;
        }
        QPushButton#refreshButton:hover, QPushButton#pageButton:hover, QPushButton#jumpButton:hover {
            background-color: #1976D2;
        }
        QPushButton#refreshButton:pressed, QPushButton#pageButton:pressed, QPushButton#jumpButton:pressed {
            background-color: #0D47A1;
        }
        QPushButton#refreshButton:disabled, QPushButton#pageButton:disabled, QPushButton#jumpButton:disabled {
            background-color: #BDBDBD;
            color: #757575;
        }
        QLineEdit#searchEdit, QLineEdit#jumpPageEdit {
            border: 2px solid #BDBDBD;
            border-radius: 8px;
            padding: 12px 16px;
            font-size: 16px;
            background-color: #fff;
            transition: border-color 0.2s;
        }
        QLineEdit#searchEdit:focus, QLineEdit#jumpPageEdit:focus {
            border-color: #2196F3;
            background: #E3F2FD;
        }
        QComboBox#levelFilter, QComboBox#sourceFilter {
            border: 2px solid #BDBDBD;
            border-radius: 8px;
            padding: 10px 16px;
            background-color: #fff;
            font-size: 16px;
            min-width: 140px;
        }
        QComboBox#levelFilter:focus, QComboBox#sourceFilter:focus {
            border-color: #2196F3;
            background: #E3F2FD;
        }
        QComboBox#levelFilter::drop-down, QComboBox#sourceFilter::drop-down {
            border: none;
            width: 24px;
        }
        QComboBox#levelFilter::down-arrow, QComboBox#sourceFilter::down-arrow {
            image: none;
            border-left: 6px solid transparent;
            border-right: 6px solid transparent;
            border-top: 6px solid #757575;
        }
        QLabel#filterLabel {
            color: #212121;
            font-weight: bold;
            font-size: 16px;
        }
        QLabel#totalCountLabel, QLabel#pageInfoLabel, QLabel#jumpLabel {
            color: #757575;
            font-size: 15px;
        }
        QTableView#logTable {
            background-color: #fff;
            border: none;
            gridline-color: #E0E0E0;
            selection-background-color: #E3F2FD;
            alternate-background-color: #FAFAFA;
            font-size: 16px;
            border-radius: 0 0 16px 16px;
        }
        QTableView#logTable::item {
            padding: 10px 6px;
            border-bottom: 1px solid #F5F5F5;
            text-overflow: ellipsis;
        }
        QTableView#logTable::item:selected {
            background-color: #BBDEFB;
            color: #1976D2;
        }
        QTableView#logTable::item:hover {
            background: #E3F2FD;
        }
        QHeaderView::section {
            background-color: #F5F5F5;
            color: #212121;
            padding: 14px 10px;
            border: none;
            border-bottom: 2px solid #E0E0E0;
            font-weight: bold;
            font-size: 16px;
            border-radius: 12px 12px 0 0;
        }
        QHeaderView::section:hover {
            background-color: #E0E0E0;
        }
        QScrollBar:vertical {
            background: #F5F5F5;
            width: 12px;
            margin: 0px 0px 0px 0px;
            border-radius: 6px;
        }
        QScrollBar::handle:vertical {
            background: #BDBDBD;
            min-height: 24px;
            border-radius: 6px;
        }
        QScrollBar::handle:vertical:hover {
            background: #9E9E9E;
        }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {
            height: 0px;
        }
        QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical {
            background: none;
        }
        QScrollBar:horizontal {
            background: #F5F5F5;
            height: 12px;
            margin: 0px 0px 0px 0px;
            border-radius: 6px;
        }
        QScrollBar::handle:horizontal {
            background: #BDBDBD;
            min-width: 24px;
            border-radius: 6px;
        }
        QScrollBar::handle:horizontal:hover {
            background: #9E9E9E;
        }
        QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal {
            width: 0px;
        }
        QScrollBar::add-page:horizontal, QScrollBar::sub-page:horizontal {
            background: none;
        }
    )");
    // 表格内容溢出省略号，悬停显示tooltip由LogModel已实现
    table->setAlternatingRowColors(true);
    table->setWordWrap(false);
    table->setTextElideMode(Qt::ElideRight);
}

void LogView::showEvent(QShowEvent *event) {
    QWidget::showEvent(event);
    setupScrollConnection();
    qDebug() << "[SHOWEVENT] showEvent触发setupScrollConnection";
}

void LogView::setupScrollConnection() {
    // 每页固定20行，不需要滚动加载更多数据
    // 移除滚动条监听逻辑
    qDebug() << "[SCROLL] 每页固定20行，已移除滚动加载逻辑";
}

void LogView::setupConnections() {
    connect(refreshBtn, &QPushButton::clicked, this, &LogView::onRefreshClicked);
    connect(apiClient, &ApiClient::logsReceived, this, [this](const QJsonArray &logs, int totalCount) {
        this->onLogsReceived(logs, totalCount, currentOffset);
    });
    connect(searchEdit, &QLineEdit::textChanged, this, &LogView::onSearchTextChanged);
    connect(levelFilter, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &LogView::onFilterChanged);
    connect(sourceFilter, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &LogView::onFilterChanged);
    connect(prevPageBtn, &QPushButton::clicked, this, [this]() {
        if (currentOffset >= pageSize) {
            currentOffset -= pageSize;
            applyFilters(true);
        }
    });
    connect(nextPageBtn, &QPushButton::clicked, this, [this]() {
        if (currentOffset + pageSize < totalCount) {
            currentOffset += pageSize;
            applyFilters(true);
        }
    });
    connect(jumpPageBtn, &QPushButton::clicked, this, &LogView::onJumpPage);
    connect(jumpPageEdit, &QLineEdit::returnPressed, this, &LogView::onJumpPage);
}

void LogView::onRefreshClicked() {
    currentOffset = 0;
    isLoading = false;
    applyFilters(true);
}

void LogView::onSearchTextChanged(const QString &text) {
    QTimer::singleShot(300, this, &LogView::onFilterChanged);
}

void LogView::onFilterChanged() {
    currentOffset = 0;
    isLoading = false;
    applyFilters(true);
}

void LogView::applyFilters(bool reset) {
    currentSearchText = searchEdit->text();
    currentLevelFilter = levelFilter->currentData().toString();
    currentSourceFilter = sourceFilter->currentData().toString();
    
    QUrlQuery query;
    query.addQueryItem("limit", QString::number(pageSize));
    query.addQueryItem("offset", QString::number(currentOffset));
    if (!currentSearchText.isEmpty()) {
        query.addQueryItem("query", currentSearchText);
    }
    if (!currentLevelFilter.isEmpty()) {
        query.addQueryItem("level", currentLevelFilter);
    }
    if (!currentSourceFilter.isEmpty()) {
        query.addQueryItem("source", currentSourceFilter);
    }
    
    qDebug() << "[APPLY] applyFilters called: currentOffset=" << currentOffset << ", reset=" << reset;
    qDebug() << "发送查询请求: limit=" << pageSize << ", offset=" << currentOffset
             << ", search=" << currentSearchText
             << ", level=" << currentLevelFilter
             << ", source=" << currentSourceFilter
             << ", isLoading=" << isLoading << ", reset=" << reset;
    
    if (reset && currentOffset == 0) {
        requestedOffsets.clear();
        model->setTotalCount(0);
    }
    
    apiClient->getLogs(query);
}

void LogView::onLogsReceived(const QJsonArray &logs, int totalCount, int offset) {
    this->totalCount = totalCount;
    
    // 只显示当前页的20条数据
    model->setTotalCount(20); // 固定每页20行
    model->setLogs(0, logs);
    
    totalCountLabel->setText(QString("总日志数: %1").arg(totalCount));
    
    int pageNum = currentOffset / pageSize + 1;
    int pageCount = (totalCount + pageSize - 1) / pageSize;
    pageInfoLabel->setText(QString("第%1页/共%2页").arg(pageNum).arg(pageCount));
    
    // 更新跳转输入框的占位符
    jumpPageEdit->setPlaceholderText(QString("%1/%2").arg(pageNum).arg(pageCount));
    
    prevPageBtn->setEnabled(currentOffset > 0);
    nextPageBtn->setEnabled(currentOffset + pageSize < totalCount);
    
    table->viewport()->update();
}

void LogView::onJumpPage() {
    bool ok = false;
    int pageNum = jumpPageEdit->text().toInt(&ok);
    int pageCount = (totalCount + pageSize - 1) / pageSize;
    
    if (ok && pageNum >= 1 && pageNum <= pageCount) {
        jumpPageEdit->setStyleSheet("QLineEdit { border: 2px solid #E0E0E0; border-radius: 4px; padding: 8px 12px; font-size: 14px; background-color: white; }");
        currentOffset = (pageNum - 1) * pageSize;
        applyFilters(true);
    } else {
        jumpPageEdit->setText("");
        jumpPageEdit->setStyleSheet("QLineEdit { border: 2px solid #F44336; border-radius: 4px; padding: 8px 12px; font-size: 14px; background-color: white; }");
        jumpPageEdit->setPlaceholderText("无效页码");
    }
}

} // namespace xumj 