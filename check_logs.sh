#!/bin/bash
SYSTEM_LOG=$1
echo "MySQL中的日志记录:"
mysql -u root -pytfhqqkso1 -e "SELECT COUNT(*) FROM log_analysis.log_entries;"
echo "表结构:"
mysql -u root -pytfhqqkso1 -e "DESCRIBE log_analysis.log_entries;"
echo "最新5条日志记录:"
mysql -u root -pytfhqqkso1 -e "SELECT id, level, source, created_at FROM log_analysis.log_entries ORDER BY created_at DESC LIMIT 5;"
