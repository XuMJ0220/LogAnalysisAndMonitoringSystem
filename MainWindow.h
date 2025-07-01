#pragma once
#include <QMainWindow>
#include <QStackedWidget>
#include <QListWidget>
#include <QLabel>
#include <QStatusBar>
#include "ApiClient.h"
#include "views/LogView.h"
#include "views/AlertView.h"
#include "views/RuleView.h"
#include "views/MonitorView.h"
#include "views/CollectorView.h"

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    MainWindow(QWidget *parent = nullptr);

private slots:
    void onHealthStatusReceived(bool healthy, const QString &message);
    void onErrorOccurred(const QString &errorMessage);
    void onNavItemClicked(int index);

private:
    xumj::ApiClient *apiClient;
    QStackedWidget *stackedWidget;
    QListWidget *navList;
    QLabel *statusLabel;
    QLabel *statusIndicator;
    xumj::LogView *logView;
    xumj::AlertView *alertView;
    xumj::RuleView *ruleView;
    xumj::MonitorView *monitorView;
    xumj::CollectorView *collectorView;
    
    void setupUi();
    void setupConnections();
    void applyMaterialDesignStyle();
    void createTopNavigationBar(QVBoxLayout *mainLayout);
    void createBottomStatusBar();
}; 