  这是一个主要以C++实现的日志分析与监控工具。前端用QT制作，用户可以直接点击导入需要分析的日志，前端可以直观得到经过后端服务器返回来的对日志分析的结果以及各类评价指标，且可以将其以邮件的方式发出。后端将前端传来的日志通过日志采集服务器、日志处理服务器、日志分析服务器和日志告警服务器处理后，将结果显示到前端以及持久化到MySQL和Redis。
