#pragma once
#include <QWidget>
#include <QPushButton>
#include <QLabel>
#include <QLineEdit>
#include <QComboBox>
#include <QTableView>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTimer>
#include <QToolButton>
#include "CollectorLogModel.h"
#include <QTcpSocket>

namespace xumj {

class CollectorView : public QWidget {
    Q_OBJECT
public:
    explicit CollectorView(QWidget *parent = nullptr);
    ~CollectorView();

signals:
    // 采集控制信号
    void startCollectRequested(int intervalMs, int maxLinesPerRound, const QString &filePath);
    void stopCollectRequested();

public slots:
    // 日志数据刷新
    void onCollectStatusChanged(bool running, const QString &statusMsg);
    void onStatsUpdated(int total, int queueLen);
    void onBrowseFile();
    void onSocketConnected();
    void onSocketDisconnected();
    void onSocketReadyRead();
    void onSocketError(QAbstractSocket::SocketError);
    void onDeleteLog();

private:
    void setupUi();
    void setupConnections();
    void sendStartCommand();
    void updatePageInfo();

    QPushButton *startBtn;
    QPushButton *stopBtn;
    QPushButton *deleteBtn;
    QLabel *statusLabel;
    QLabel *statsLabel;
    QLineEdit *filePathEdit;
    QToolButton *browseBtn;
    QLineEdit *intervalEdit;
    QLineEdit *maxLinesEdit;
    QTableView *table;
    CollectorLogModel *model;
    QTcpSocket *socket;
    QString serverIp;
    quint16 serverPort;

    // 分页相关
    QPushButton *refreshBtn;
    QPushButton *prevPageBtn;
    QPushButton *nextPageBtn;
    QLabel *pageInfoLabel;
    QLineEdit *jumpPageEdit;
    QPushButton *jumpPageBtn;
    int currentPage_ = 0;
    int pageSize_ = 20;
};

} // namespace xumj 