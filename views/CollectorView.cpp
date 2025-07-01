#include "CollectorView.h"
#include <QWidget>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QHeaderView>
#include <QDateTime>
#include <QFileDialog>
#include <QTcpSocket>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPushButton>
#include <QLabel>
#include <QTableView>
#include <QToolButton>
#include <QLineEdit>
#include <QTableView>
#include <QAbstractItemView>
#include <QMessageBox>
#include <QDebug>
#include <algorithm>
#include <QFile>
#include <QApplication>
#include <QJsonArray>

namespace xumj {

CollectorView::CollectorView(QWidget *parent)
    : QWidget(parent), socket(new QTcpSocket(this)), serverIp("127.0.0.1"), serverPort(9000) {
    setupUi();
    setupConnections();
    connect(socket, &QTcpSocket::connected, this, &CollectorView::onSocketConnected);
    connect(socket, &QTcpSocket::disconnected, this, &CollectorView::onSocketDisconnected);
    connect(socket, &QTcpSocket::readyRead, this, &CollectorView::onSocketReadyRead);
    connect(socket, QOverload<QAbstractSocket::SocketError>::of(&QTcpSocket::error), this, &CollectorView::onSocketError);
    // 加载QSS样式表
    QFile file("resources/qss/modern.qss");
    if (file.open(QFile::ReadOnly)) {
        QString styleSheet = QLatin1String(file.readAll());
        qApp->setStyleSheet(styleSheet);
    }
    // 表格美化
    table->setAlternatingRowColors(true);
    table->setStyleSheet("QTableView { selection-background-color: #409EFF; border: none; } QHeaderView::section { font-weight: bold; background: #f5f7fa; border: none; } QTableView::item:selected { color: white; }");
    table->horizontalHeader()->setHighlightSections(false);
    table->verticalHeader()->setVisible(false);
    table->setShowGrid(false);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setSelectionMode(QAbstractItemView::SingleSelection);
    // 按钮美化
    startBtn->setStyleSheet("QPushButton { background: #409EFF; color: white; border-radius: 4px; padding: 6px 16px; } QPushButton:hover { background: #66b1ff; } QPushButton:pressed { background: #337ecc; }");
    stopBtn->setStyleSheet("QPushButton { background: #909399; color: white; border-radius: 4px; padding: 6px 16px; } QPushButton:hover { background: #b1b3b8; } QPushButton:pressed { background: #606266; }");
    deleteBtn->setStyleSheet("QPushButton { background: #f56c6c; color: white; border-radius: 4px; padding: 6px 16px; } QPushButton:hover { background: #ff8787; } QPushButton:pressed { background: #c45656; }");
    // 分页控件美化
    prevPageBtn->setStyleSheet("QPushButton { background: #ecf5ff; color: #409EFF; border-radius: 4px; padding: 4px 12px; } QPushButton:hover { background: #d9ecff; }");
    nextPageBtn->setStyleSheet("QPushButton { background: #ecf5ff; color: #409EFF; border-radius: 4px; padding: 4px 12px; } QPushButton:hover { background: #d9ecff; }");
    pageInfoLabel->setStyleSheet("QLabel { color: #409EFF; font-size: 20px; font-weight: bold; }");
    // 统计区块美化
    statsLabel->setStyleSheet("QLabel { font-weight: bold; font-size: 16px; color: #303133; }");
}

CollectorView::~CollectorView() {}

void CollectorView::setupUi() {
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(24, 24, 24, 24);
    mainLayout->setSpacing(16);

    // 顶部工具栏
    QHBoxLayout *toolbarLayout = new QHBoxLayout();
    refreshBtn = new QPushButton("刷新采集", this);
    toolbarLayout->addWidget(refreshBtn);
    toolbarLayout->addStretch();
    statsLabel = new QLabel("采集统计: 0 条", this);
    toolbarLayout->addWidget(statsLabel);
    mainLayout->addLayout(toolbarLayout);

    // 参数设置区
    QHBoxLayout *paramLayout = new QHBoxLayout();
    filePathEdit = new QLineEdit(this);
    filePathEdit->setPlaceholderText("日志文件路径");
    browseBtn = new QToolButton(this);
    browseBtn->setIcon(QIcon::fromTheme("folder-open"));
    browseBtn->setToolTip("选择日志文件");
    paramLayout->addWidget(new QLabel("文件:", this));
    paramLayout->addWidget(filePathEdit);
    paramLayout->addWidget(browseBtn);
    paramLayout->addWidget(new QLabel("间隔:", this));
    intervalEdit = new QLineEdit(this);
    intervalEdit->setPlaceholderText("采集间隔(ms)");
    intervalEdit->setText("1000");
    paramLayout->addWidget(intervalEdit);
    paramLayout->addWidget(new QLabel("条数:", this));
    maxLinesEdit = new QLineEdit(this);
    maxLinesEdit->setPlaceholderText("每轮采集条数");
    maxLinesEdit->setText("10");
    paramLayout->addWidget(maxLinesEdit);
    paramLayout->addStretch();
    mainLayout->addLayout(paramLayout);

    // 采集控制按钮
    QHBoxLayout *ctrlLayout = new QHBoxLayout();
    startBtn = new QPushButton("启动采集", this);
    stopBtn = new QPushButton("停止采集", this);
    deleteBtn = new QPushButton("删除选中行", this);
    statusLabel = new QLabel("采集状态: 未启动", this);
    ctrlLayout->addWidget(startBtn);
    ctrlLayout->addWidget(stopBtn);
    ctrlLayout->addWidget(deleteBtn);
    ctrlLayout->addStretch();
    ctrlLayout->addWidget(statusLabel);
    mainLayout->addLayout(ctrlLayout);

    // 日志表格
    table = new QTableView(this);
    model = new CollectorLogModel(this);
    table->setModel(model);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setAlternatingRowColors(true);
    table->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    table->verticalHeader()->setDefaultSectionSize(40);
    table->verticalHeader()->setVisible(false);
    table->setColumnWidth(0, 160);
    table->setColumnWidth(1, 80);
    table->setColumnWidth(2, 600);
    for (int i = 3; i < 10; ++i) {
        table->setColumnHidden(i, true);
    }
    mainLayout->addWidget(table, 1);

    // 分页控件
    QHBoxLayout *pageLayout = new QHBoxLayout();
    prevPageBtn = new QPushButton("上一页", this);
    nextPageBtn = new QPushButton("下一页", this);
    pageInfoLabel = new QLabel("第1页/共1页", this);
    jumpPageEdit = new QLineEdit(this);
    jumpPageEdit->setFixedWidth(60);
    jumpPageEdit->setPlaceholderText("页码");
    jumpPageBtn = new QPushButton("跳转", this);
    pageLayout->addWidget(prevPageBtn);
    pageLayout->addWidget(nextPageBtn);
    pageLayout->addWidget(pageInfoLabel);
    pageLayout->addWidget(jumpPageEdit);
    pageLayout->addWidget(jumpPageBtn);
    pageLayout->addStretch();
    mainLayout->addLayout(pageLayout);
}

void CollectorView::setupConnections() {
    connect(refreshBtn, &QPushButton::clicked, this, [this]() {
        // 刷新当前页
        model->setPage(currentPage_);
        updatePageInfo();
    });
    connect(prevPageBtn, &QPushButton::clicked, this, [this]() {
        if (currentPage_ > 0) {
            --currentPage_;
            model->setPage(currentPage_);
            updatePageInfo();
        }
    });
    connect(nextPageBtn, &QPushButton::clicked, this, [this]() {
        int pageCount = (model->totalCount() + pageSize_ - 1) / pageSize_;
        if (currentPage_ + 1 < pageCount) {
            ++currentPage_;
            model->setPage(currentPage_);
            updatePageInfo();
        }
    });
    connect(jumpPageBtn, &QPushButton::clicked, this, [this]() {
        bool ok = false;
        int pageNum = jumpPageEdit->text().toInt(&ok);
        int pageCount = (model->totalCount() + pageSize_ - 1) / pageSize_;
        if (ok && pageNum >= 1 && pageNum <= pageCount) {
            currentPage_ = pageNum - 1;
            model->setPage(currentPage_);
            updatePageInfo();
        } else {
            jumpPageEdit->setText("");
            jumpPageEdit->setPlaceholderText("无效页码");
        }
    });
    connect(jumpPageEdit, &QLineEdit::returnPressed, jumpPageBtn, &QPushButton::click);
    connect(startBtn, &QPushButton::clicked, this, [this]() {
        if (socket->state() == QAbstractSocket::ConnectedState) {
            sendStartCommand();
        } else {
            socket->connectToHost(serverIp, serverPort);
        }
    });
    connect(stopBtn, &QPushButton::clicked, this, [this]() {
        if (socket->state() == QAbstractSocket::ConnectedState) {
            QJsonObject cmdObj;
            cmdObj["cmd"] = "stop";
            QByteArray data = QJsonDocument(cmdObj).toJson(QJsonDocument::Compact) + "\r\n";
            socket->write(data);
            socket->disconnectFromHost();
        }
    });
    connect(browseBtn, &QToolButton::clicked, this, &CollectorView::onBrowseFile);
    connect(deleteBtn, &QPushButton::clicked, this, &CollectorView::onDeleteLog);
    connect(model, &CollectorLogModel::statsChanged, this, [this](int count) {
        statsLabel->setText(QString("采集统计: %1 条, 队列 %2").arg(count).arg(0));
    });
}

void CollectorView::onSocketConnected() {
    statusLabel->setText("采集状态: 已连接");
    sendStartCommand();
    // 启动采集后刷新统计
    statsLabel->setText(QString("采集统计: %1 条, 队列 %2").arg(model->logCount()).arg(0));
}

void CollectorView::onSocketDisconnected() {
    statusLabel->setText("采集状态: 已断开");
    // 停止采集后刷新统计
    statsLabel->setText(QString("采集统计: %1 条, 队列 %2").arg(model->logCount()).arg(0));
}

void CollectorView::onSocketReadyRead() {
    QVector<CollectorLogEntry> allLogs = model->allLogs();
    while (socket->canReadLine()) {
        QByteArray line = socket->readLine().trimmed();
        qDebug() << "收到原始数据:" << line;
        QJsonParseError err;
        QJsonDocument doc = QJsonDocument::fromJson(line, &err);
        if (err.error == QJsonParseError::NoError) {
            if (doc.isObject()) {
                QJsonObject obj = doc.object();
                CollectorLogEntry entry;
                entry.time = obj.value("time").toString();
                entry.level = obj.value("level").toString();
                entry.content = obj.value("content").toString();
                allLogs.append(entry);
            } else if (doc.isArray()) {
                QJsonArray arr = doc.array();
                for (const QJsonValue &val : arr) {
                    if (val.isObject()) {
                        QJsonObject obj = val.toObject();
                        CollectorLogEntry entry;
                        entry.time = obj.value("time").toString();
                        entry.level = obj.value("level").toString();
                        entry.content = obj.value("content").toString();
                        allLogs.append(entry);
                    }
                }
            }
        } else {
            qDebug() << "JSON解析失败:" << err.errorString();
        }
    }
    model->setAllLogs(allLogs);
    model->setPage(currentPage_);
    updatePageInfo();
}

void CollectorView::onSocketError(QAbstractSocket::SocketError err) {
    Q_UNUSED(err);
    statusLabel->setText("采集状态: 连接错误");
    qDebug() << "Socket error:" << socket->errorString();
    QMessageBox::warning(this, "Socket错误", socket->errorString());
}

void CollectorView::onCollectStatusChanged(bool running, const QString &statusMsg) {
    statusLabel->setText(QString("采集状态: %1").arg(running ? "运行中" : "已停止"));
    if (!statusMsg.isEmpty()) {
        statusLabel->setText(statusLabel->text() + " (" + statusMsg + ")");
    }
}

void CollectorView::onStatsUpdated(int total, int queueLen) {
    statsLabel->setText(QString("采集统计: %1 条, 队列 %2").arg(total).arg(queueLen));
}

void CollectorView::onBrowseFile() {
    QString file = QFileDialog::getOpenFileName(this, "选择日志文件", "", "日志文件 (*.log *.txt);;所有文件 (*)");
    if (!file.isEmpty()) {
        filePathEdit->setText(file);
    }
}

void CollectorView::sendStartCommand() {
    int interval = intervalEdit->text().toInt();
    int maxLines = maxLinesEdit->text().toInt();
    QString filePath = filePathEdit->text();
    QJsonObject cmdObj;
    cmdObj["cmd"] = "start";
    cmdObj["file"] = filePath;
    cmdObj["interval"] = interval;
    cmdObj["maxLines"] = maxLines;
    QByteArray data = QJsonDocument(cmdObj).toJson(QJsonDocument::Compact) + "\r\n";
    socket->write(data);
}

void CollectorView::onDeleteLog() {
    auto sel = table->selectionModel()->selectedRows();
    if (sel.isEmpty()) return;
    // 支持批量删除
    std::sort(sel.begin(), sel.end(), [](const QModelIndex &a, const QModelIndex &b) { return a.row() > b.row(); });
    for (const QModelIndex &idx : sel) {
        model->removeRows(idx.row(), 1);
    }
}

void CollectorView::updatePageInfo() {
    int total = model->totalCount();
    int pageCount = (total + pageSize_ - 1) / pageSize_;
    pageInfoLabel->setText(QString("第%1页/共%2页").arg(currentPage_ + 1).arg(pageCount));
    jumpPageEdit->setPlaceholderText(QString("%1/%2").arg(currentPage_ + 1).arg(pageCount));
    prevPageBtn->setEnabled(currentPage_ > 0);
    nextPageBtn->setEnabled(currentPage_ + 1 < pageCount);
    statsLabel->setText(QString("采集统计: %1 条").arg(total));
}

} // namespace xumj 