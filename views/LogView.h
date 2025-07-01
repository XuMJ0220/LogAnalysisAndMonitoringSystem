#pragma once
#include <QWidget>
#include "DraggableTableView.h"
#include <QPushButton>
#include <QLineEdit>
#include <QComboBox>
#include <QLabel>
#include <QHBoxLayout>
#include "../models/LogModel.h"
#include "../ApiClient.h"

namespace xumj {

class LogView : public QWidget {
    Q_OBJECT
public:
    LogView(ApiClient *api, QWidget *parent = nullptr);
protected:
    void showEvent(QShowEvent *event) override;
private slots:
    void onLogsReceived(const QJsonArray &logs, int totalCount, int offset = 0);
    void onFilterChanged();
    void onRefreshClicked();
    void onSearchTextChanged(const QString &text);
    void onJumpPage();
private:
    void setupUi();
    void setupConnections();
    void setupMaterialDesignStyle();
    void createToolbarCard(QVBoxLayout *mainLayout);
    void createFilterCard(QVBoxLayout *mainLayout);
    void createTableCard(QVBoxLayout *mainLayout);
    void createPaginationCard(QVBoxLayout *mainLayout);
    void setupScrollConnection();
    void applyFilters(bool reset);

    DraggableTableView *table;
    LogModel *model;
    ApiClient *apiClient;
    QPushButton *refreshBtn;
    QPushButton *prevPageBtn;
    QPushButton *nextPageBtn;
    QLabel *pageInfoLabel;
    
    // 过滤控件
    QLineEdit *searchEdit;
    QComboBox *levelFilter;
    QComboBox *sourceFilter;
    QLabel *totalCountLabel;
    
    // 过滤状态
    QString currentSearchText;
    QString currentLevelFilter;
    QString currentSourceFilter;
    int currentOffset;
    int pageSize;
    bool isLoading;
    QSet<int> requestedOffsets; // 记录已请求过的offset
    int totalCount; // 新增，保存总条数
    QLineEdit *jumpPageEdit;
    QPushButton *jumpPageBtn;
};

} // namespace xumj 