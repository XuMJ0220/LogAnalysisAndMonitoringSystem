# config.json 配置项说明

- **logs_per_second**：每秒生成的日志条数，建议10~1000，0为不限速。
- **total_logs**：总共生成多少条日志，0为无限制（与duration_seconds二选一，优先total_logs）。
- **duration_seconds**：持续生成日志的秒数，0为无限制。
- **output_file**：日志输出文件路径（相对或绝对路径均可）。
- **level_distribution**：日志级别分布，百分比之和应为100。例如{"info":70,"warn":20,"error":10}。
- **modules**：参与mock的业务模块列表，如["user","order","payment","system"]。
- **locale**：mock数据语言（en为英文，zh_CN为中文等）。
- **log_format**：日志输出格式模板，可用变量：{time} {level} {module} {msg}等。
- **console_output**：是否同时输出到控制台（true为输出，false为仅写文件）。
- **fields**：mock字段开关，决定每条日志包含哪些字段，如["name","phone","company","ip","action","sentence"]。

> 注意：config.json 必须为纯 JSON 格式，不能包含 // 或 /* ... */ 注释，否则程序无法解析。 