#include "MainWindow.h"
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QTimer>
#include <QMessageBox>
#include <QApplication>
#include <QStyle>
#include <QSplitter>
#include <QFrame>
#include <QGraphicsDropShadowEffect>
#include <QStatusBar>
#include <QLabel>

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent) {
    apiClient = new xumj::ApiClient(this);
    setupUi();
    setupConnections();
    applyMaterialDesignStyle();
    apiClient->checkHealth();
    QTimer *healthTimer = new QTimer(this);
    connect(healthTimer, &QTimer::timeout, apiClient, &xumj::ApiClient::checkHealth);
    healthTimer->start(30000);
}

void MainWindow::setupUi() {
    // 设置窗口基本属性
    setWindowTitle("分布式日志监控系统");
    setMinimumSize(1200, 800);
    
    // 创建中央控件
    QWidget *centralWidget = new QWidget(this);
    centralWidget->setObjectName("mainWindow");
    
    // 创建主布局
    QVBoxLayout *mainLayout = new QVBoxLayout(centralWidget);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);
    
    // 创建顶部导航栏
    createTopNavigationBar(mainLayout);
    
    // 创建主内容区域 - 使用QSplitter
    QSplitter *splitter = new QSplitter(Qt::Horizontal, centralWidget);
    splitter->setObjectName("mainSplitter");
    
    navList = new QListWidget(splitter);
    navList->setObjectName("navList");
    navList->addItem(new QListWidgetItem(QIcon::fromTheme("text-x-generic"), "日志监控"));
    navList->addItem(new QListWidgetItem(QIcon::fromTheme("dialog-warning"), "告警管理"));
    navList->addItem(new QListWidgetItem(QIcon::fromTheme("preferences-system"), "规则配置"));
    navList->addItem(new QListWidgetItem(QIcon::fromTheme("utilities-system-monitor"), "系统监控"));
    navList->addItem(new QListWidgetItem(QIcon::fromTheme("folder-download"), "采集监控"));
    navList->setIconSize(QSize(24, 24));
    navList->setMinimumWidth(200);
    navList->setMaximumWidth(300);
    
    stackedWidget = new QStackedWidget(splitter);
    stackedWidget->setObjectName("stackedWidget");
    
    // 添加日志视图
    logView = new xumj::LogView(apiClient, stackedWidget);
    stackedWidget->addWidget(logView);
    
    // 添加告警视图
    alertView = new xumj::AlertView(apiClient, stackedWidget);
    stackedWidget->addWidget(alertView);
    
    // 添加规则配置视图
    ruleView = new xumj::RuleView(apiClient, stackedWidget);
    stackedWidget->addWidget(ruleView);
    
    // 添加系统监控视图
    monitorView = new xumj::MonitorView(apiClient, stackedWidget);
    stackedWidget->addWidget(monitorView);
    
    // 添加采集监控视图
    collectorView = new xumj::CollectorView(stackedWidget);
    stackedWidget->addWidget(collectorView);
    
    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 1);
    splitter->setStretchFactor(2, 0);
    splitter->setStretchFactor(3, 0);
    splitter->setStretchFactor(4, 0);
    splitter->setSizes({250, 800, 200, 200, 200});
    
    mainLayout->addWidget(splitter, 1);
    
    // 创建底部状态栏
    createBottomStatusBar();
    
    setCentralWidget(centralWidget);
}

void MainWindow::createTopNavigationBar(QVBoxLayout *mainLayout) {
    QFrame *topBar = new QFrame(this);
    topBar->setObjectName("topNavigationBar");
    topBar->setFixedHeight(64);
    
    QHBoxLayout *topLayout = new QHBoxLayout(topBar);
    topLayout->setContentsMargins(24, 0, 24, 0);
    
    // 应用标题
    QLabel *titleLabel = new QLabel("分布式日志监控系统", topBar);
    titleLabel->setObjectName("appTitle");
    titleLabel->setStyleSheet("font-size: 20px; font-weight: bold; color: white;");
    
    topLayout->addWidget(titleLabel);
    topLayout->addStretch();
    
    // 系统状态指示器
    statusIndicator = new QLabel("●", topBar);
    statusIndicator->setObjectName("statusIndicator");
    statusIndicator->setStyleSheet("font-size: 16px; color: #FF9800;");
    
    statusLabel = new QLabel("连接中...", topBar);
    statusLabel->setObjectName("statusLabel");
    statusLabel->setStyleSheet("color: white; font-size: 14px;");
    
    topLayout->addWidget(statusIndicator);
    topLayout->addWidget(statusLabel);
    
    mainLayout->addWidget(topBar);
}

void MainWindow::createBottomStatusBar() {
    QStatusBar *statusBar = new QStatusBar(this);
    QLabel *copyrightLabel = new QLabel("© 2024 许铭杰", statusBar);
    copyrightLabel->setStyleSheet("color: #757575; font-size: 18px;");
    statusBar->addPermanentWidget(copyrightLabel, 1); // 居中填充
    setStatusBar(statusBar);
}

void MainWindow::applyMaterialDesignStyle() {
    setStyleSheet(R"(
        QWidget#mainWindow {
            background-color: #FAFAFA;
        }
        
        QFrame#topNavigationBar {
            background: qlineargradient(x1:0, y1:0, x2:1, y2:0, 
                stop:0 #2196F3, stop:1 #1976D2);
            border: none;
        }
        
        QFrame#sideNavigation {
            background-color: white;
            border-radius: 8px;
        }
        
        QFrame#mainContent {
            background-color: white;
            border-radius: 8px;
        }
        
        QFrame#contentHeader {
            background-color: white;
            border-bottom: 1px solid #E0E0E0;
        }
        
        QFrame#bottomStatusBar {
            background-color: white;
            border-top: 1px solid #E0E0E0;
        }
        
        QListWidget#navList {
            background-color: transparent;
            border: none;
            outline: none;
        }
        
        QListWidget#navList::item {
            background-color: transparent;
            border: none;
            padding: 12px 16px;
            border-radius: 4px;
            margin: 2px 8px;
        }
        
        QListWidget#navList::item:hover {
            background-color: #E3F2FD;
        }
        
        QListWidget#navList::item:selected {
            background-color: #2196F3;
            color: white;
        }
        
        QStackedWidget#stackedWidget {
            background-color: transparent;
            border: none;
        }
    )");
}

void MainWindow::setupConnections() {
    connect(navList, &QListWidget::currentRowChanged, this, &MainWindow::onNavItemClicked);
    navList->setCurrentRow(0);
    connect(apiClient, &xumj::ApiClient::healthStatusReceived, this, &MainWindow::onHealthStatusReceived);
    connect(apiClient, &xumj::ApiClient::errorOccurred, this, &MainWindow::onErrorOccurred);
}

void MainWindow::onNavItemClicked(int index) {
    stackedWidget->setCurrentIndex(index);
}

void MainWindow::onHealthStatusReceived(bool healthy, const QString &message) {
    if (healthy) {
        statusLabel->setText("系统状态: 正常");
        statusLabel->setStyleSheet("color: white; font-size: 14px;");
        statusIndicator->setStyleSheet("font-size: 16px; color: #4CAF50;");
    } else {
        statusLabel->setText("系统状态: 异常 - " + message);
        statusLabel->setStyleSheet("color: white; font-size: 14px;");
        statusIndicator->setStyleSheet("font-size: 16px; color: #F44336;");
    }
}

void MainWindow::onErrorOccurred(const QString &errorMessage) {
    QMessageBox::warning(this, "错误", "发生错误: " + errorMessage);
} 