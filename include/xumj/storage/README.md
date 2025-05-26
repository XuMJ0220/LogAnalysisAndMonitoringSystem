# 存储模块 (Storage Module)

这个模块提供了对多种存储后端的统一访问接口，目前支持Redis和MySQL两种存储系统。

## 模块结构

存储模块由以下主要组件组成：

1. **RedisStorage**: Redis存储实现，用于高速缓存和临时数据存储
   - 支持字符串、列表、哈希表、集合等Redis数据类型
   - 提供连接池管理，优化性能和资源利用
   - 包含错误处理和重连机制

2. **MySQLStorage**: MySQL存储实现，用于持久化数据存储
   - 提供日志数据的结构化存储
   - 支持复杂查询和聚合操作
   - 包含连接池管理和错误处理

3. **StorageFactory**: 存储工厂类，用于创建和管理存储实例
   - 支持通过JSON配置创建存储实例
   - 提供存储实例的注册和获取功能
   - 支持多种存储类型的统一管理

## 主要功能

### Redis存储 (RedisStorage)

- 基本键值操作：`Set`/`Get`/`Delete`/`Exists`/`Expire`
- 列表操作：`ListPush`/`ListPushFront`/`ListPop`/`ListPopFront`/`ListLength`/`ListRange`
- 哈希表操作：`HashSet`/`HashGet`/`HashDelete`/`HashExists`/`HashGetAll`
- 集合操作：`SetAdd`/`SetRemove`/`SetIsMember`/`SetMembers`/`SetSize`
- 事务操作：`Multi`/`Exec`/`Discard`（注意：在连接池环境中需要特殊处理）
- 连接管理：`Ping`/`Info`

### MySQL存储 (MySQLStorage)

- 数据库初始化：`Initialize`
- 日志条目操作：`SaveLogEntry`/`SaveLogEntries`/`GetLogEntryById`
- 查询功能：
  - 按条件查询：`QueryLogEntries`
  - 按时间范围查询：`QueryLogEntriesByTimeRange`
  - 按日志级别查询：`QueryLogEntriesByLevel`
  - 按日志来源查询：`QueryLogEntriesBySource`
  - 全文搜索：`SearchLogEntriesByKeyword`
- 管理功能：`GetLogEntryCount`/`DeleteLogEntriesBefore`

### 存储工厂 (StorageFactory)

- 存储实例创建：`CreateRedisStorage`/`CreateMySQLStorage`/`CreateStorage`
- 配置解析：`CreateRedisConfigFromJson`/`CreateMySQLConfigFromJson`
- 存储实例管理：`RegisterStorage`/`GetStorage`

## 使用指南

### 示例：Redis存储

```cpp
#include <xumj/storage/redis_storage.h>

// 创建配置
xumj::storage::RedisConfig config;
config.host = "127.0.0.1";
config.port = 6379;
config.password = "your_password"; // 如果需要
config.database = 0;
config.timeout = 3000; // 毫秒
config.poolSize = 5;   // 连接池大小

// 创建Redis存储实例
xumj::storage::RedisStorage redis(config);

// 基本操作
redis.Set("key", "value");
std::string value = redis.Get("key");
redis.Delete("key");

// 列表操作
redis.ListPush("list_key", "item1");
redis.ListPush("list_key", "item2");
auto items = redis.ListRange("list_key", 0, -1);

// 哈希表操作
redis.HashSet("hash_key", "field1", "value1");
redis.HashSet("hash_key", "field2", "value2");
auto allFields = redis.HashGetAll("hash_key");
```

### 示例：MySQL存储

```cpp
#include <xumj/storage/mysql_storage.h>

// 创建配置
xumj::storage::MySQLConfig config;
config.host = "127.0.0.1";
config.port = 3306;
config.username = "root";
config.password = "your_password";
config.database = "logs_db";
config.timeout = 5; // 秒
config.poolSize = 5; // 连接池大小

// 创建MySQL存储实例
xumj::storage::MySQLStorage mysql(config);

// 初始化数据库表结构
mysql.Initialize();

// 创建并保存日志条目
xumj::storage::MySQLStorage::LogEntry entry;
entry.id = "log-123";
entry.timestamp = "2023-05-15T10:30:00";
entry.level = "INFO";
entry.source = "test-service";
entry.message = "测试日志消息";
entry.fields["user_id"] = "12345";
entry.fields["module"] = "auth";

mysql.SaveLogEntry(entry);

// 查询日志
auto entries = mysql.QueryLogEntriesByLevel("ERROR", 10);
for (const auto& e : entries) {
    std::cout << e.timestamp << " [" << e.level << "] " << e.message << std::endl;
}
```

### 示例：存储工厂

```cpp
#include <xumj/storage/storage_factory.h>

// 创建存储工厂实例
xumj::storage::StorageFactory factory;

// 从JSON配置创建存储实例
std::string redisConfigJson = R"({"host":"127.0.0.1","port":6379,"password":"","database":0})";
auto redisConfig = xumj::storage::StorageFactory::CreateRedisConfigFromJson(redisConfigJson);
auto redisStorage = xumj::storage::StorageFactory::CreateRedisStorage(redisConfig);

// 注册存储实例到工厂
factory.RegisterStorage("main-redis", redisStorage);

// 获取存储实例
auto redis = factory.GetStorage<xumj::storage::RedisStorage>("main-redis");
if (redis) {
    redis->Set("test", "value");
}
```

## 注意事项

1. **Redis事务处理**：在连接池环境中，MULTI和EXEC命令需要在同一个连接上执行，目前的实现可能会导致事务失败。在生产环境中，建议使用Pipeline模式或修改事务实现以支持连接池。

2. **错误处理**：存储模块中的大多数方法会抛出异常（`RedisStorageException`或`MySQLStorageException`），请确保在使用时进行适当的异常处理。

3. **连接管理**：为了避免资源泄漏，存储实例会自动管理连接池的生命周期。创建过多的存储实例可能会导致性能问题。

4. **线程安全**：存储模块的实现是线程安全的，可以在多线程环境中使用。

## 示例程序

项目中提供了三个示例程序，位于`examples`目录：

1. `redis_storage_example.cpp`: 演示Redis存储的使用
2. `mysql_storage_example.cpp`: 演示MySQL存储的使用
3. `storage_factory_example.cpp`: 演示存储工厂的使用

编译和运行示例程序：

```bash
# 编译
cd build
make redis_storage_example mysql_storage_example storage_factory_example

# 运行
./bin/redis_storage_example
./bin/mysql_storage_example
./bin/storage_factory_example
``` 