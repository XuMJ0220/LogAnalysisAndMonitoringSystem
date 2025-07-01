#pragma once
#include <QWidget>
#include <QPushButton>
#include <QLineEdit>
#include <QComboBox>
#include <QLabel>
#include <QHBoxLayout>
#include <QTableView>
#include "../models/AlertModel.h"
#include "../ApiClient.h"

namespace xumj {

class AlertView : public QWidget {
    Q_OBJECT
public:
    AlertView(ApiClient *api, QWidget *parent = nullptr);
protected:
    void showEvent(QShowEvent *event) override;
private slots:
    void onAlertsReceived(const QJsonArray &alerts);
    void onFilterChanged();
    void onRefreshClicked();
    void onResolveClicked();
    void onIgnoreClicked();
    void onAlertTableClicked(const QModelIndex &index);
    void onAlertStatusUpdated(const QString &alertId, bool success);
private:
    void setupUi();
    void setupConnections();
    void setupMaterialDesignStyle();
    void createToolbarCard(QVBoxLayout *mainLayout);
    void createFilterCard(QVBoxLayout *mainLayout);
    void createTableCard(QVBoxLayout *mainLayout);
    void createDetailCard(QVBoxLayout *mainLayout);
    void loadAlerts();
    void showAlertDetail(const QJsonObject &alert);

    QTableView *table;
    AlertModel *model;
    ApiClient *apiClient;
    QPushButton *refreshBtn;
    QPushButton *resolveBtn;
    QPushButton *ignoreBtn;
    
    // 过滤控件
    QComboBox *levelFilter;
    QComboBox *sourceFilter;
    QComboBox *statusFilter;
    QLabel *totalCountLabel;
    
    // 详情控件
    QLabel *detailTitleLabel;
    QLabel *detailDescriptionLabel;
    QLabel *detailSourceLabel;
    QLabel *detailLevelLabel;
    QLabel *detailStatusLabel;
    QLabel *detailTimeLabel;
    
    // 当前选中的告警ID
    QString selectedAlertId;
};

} // namespace xumj 