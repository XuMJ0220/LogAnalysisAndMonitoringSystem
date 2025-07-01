#pragma once
#include <QWidget>
#include <QPushButton>
#include <QLabel>
#include <QTableView>
#include <QTimer>
#include "../models/MonitorModel.h"
#include "../ApiClient.h"

namespace xumj {

class MonitorView : public QWidget {
    Q_OBJECT
public:
    MonitorView(ApiClient *api, QWidget *parent = nullptr);
protected:
    void showEvent(QShowEvent *event) override;
private slots:
    void onSystemStatusReceived(const QJsonObject &status);
    void onRefreshClicked();
    void onAutoRefreshTimeout();
private:
    void setupUi();
    void setupConnections();
    void setupMaterialDesignStyle();
    void loadStatus();

    QTableView *table;
    MonitorModel *model;
    ApiClient *apiClient;
    QPushButton *refreshBtn;
    QLabel *totalCountLabel;
    QTimer *autoRefreshTimer;
};

} // namespace xumj 