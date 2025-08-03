# ESP32 MCP Server Component

这是一个 ESP32 的 MCP (Model Context Protocol) 服务器组件，提供简洁的 API 来创建自定义的 MCP 服务器，支持动态注册工具和资源。

## 特性

- 🚀 **简洁的 API**: 类似 JavaScript 风格的注册接口
- 🔧 **动态注册**: 运行时注册和管理工具与资源
- 📦 **模块化设计**: 独立的组件，易于集成
- 🌐 **HTTP 传输**: 基于 HTTP 的 JSON-RPC 2.0 协议
- 🔄 **URI 模板**: 支持参数化的资源 URI
- 📊 **统计信息**: 实时服务器状态监控
- 🛡️ **内存安全**: 自动内存管理和清理

## 快速开始

### 1. 添加组件

将 `esp_mcp_server` 组件添加到您的项目中：

```cmake
# 在您的 main/CMakeLists.txt 中
idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS "."
    REQUIRES esp_mcp_server
)
```

### 2. 基本使用

```c
#include "esp_mcp_server.h"

void app_main(void) {
    // 配置服务器
    esp_mcp_server_config_t config = ESP_MCP_SERVER_DEFAULT_CONFIG();
    config.port = 80;
    config.server_name = "My ESP32 Server";
    
    // 启动服务器
    esp_mcp_server_handle_t server;
    esp_mcp_server_start(&config, &server);
    
    // 注册工具和资源...
}
```

## API 参考

### 服务器管理

#### `esp_mcp_server_start()`

启动 MCP 服务器。

```c
esp_err_t esp_mcp_server_start(
    const esp_mcp_server_config_t *config, 
    esp_mcp_server_handle_t *server_handle
);
```

**参数:**
- `config`: 服务器配置
- `server_handle`: 输出的服务器句柄

**返回:** `ESP_OK` 成功，其他错误码表示失败

#### `esp_mcp_server_stop()`

停止并清理 MCP 服务器。

```c
esp_err_t esp_mcp_server_stop(esp_mcp_server_handle_t server_handle);
```

### 工具注册

#### `esp_mcp_server_register_tool()`

注册一个工具到 MCP 服务器。

```c
esp_err_t esp_mcp_server_register_tool(
    esp_mcp_server_handle_t server_handle, 
    const esp_mcp_tool_config_t *tool_config
);
```

**示例:**

```c
// 工具处理函数
cJSON* echo_handler(const cJSON *arguments, void *user_data) {
    cJSON *message = cJSON_GetObjectItem(arguments, "message");
    if (!message || !cJSON_IsString(message)) {
        return NULL;
    }
    
    cJSON *result = cJSON_CreateObject();
    cJSON *content_array = cJSON_CreateArray();
    cJSON *content = cJSON_CreateObject();
    
    cJSON_AddStringToObject(content, "type", "text");
    char response[256];
    snprintf(response, sizeof(response), "Echo: %s", message->valuestring);
    cJSON_AddStringToObject(content, "text", response);
    
    cJSON_AddItemToArray(content_array, content);
    cJSON_AddItemToObject(result, "content", content_array);
    
    return result;
}

// 创建输入模式
cJSON *schema = cJSON_CreateObject();
cJSON_AddStringToObject(schema, "type", "object");
cJSON *properties = cJSON_CreateObject();
cJSON *message_prop = cJSON_CreateObject();
cJSON_AddStringToObject(message_prop, "type", "string");
cJSON_AddItemToObject(properties, "message", message_prop);
cJSON_AddItemToObject(schema, "properties", properties);

// 注册工具
esp_mcp_tool_config_t tool_config = {
    .name = "echo",
    .title = "Echo Tool",
    .description = "Echoes back messages",
    .input_schema = schema,
    .handler = echo_handler,
    .user_data = NULL
};

esp_mcp_server_register_tool(server, &tool_config);
cJSON_Delete(schema); // 注册后可以安全删除
```

### 资源注册

#### `esp_mcp_server_register_resource()`

注册一个资源到 MCP 服务器。

```c
esp_err_t esp_mcp_server_register_resource(
    esp_mcp_server_handle_t server_handle, 
    const esp_mcp_resource_config_t *resource_config
);
```

**示例:**

```c
// 资源处理函数
char* sensor_handler(const char *uri, void *user_data) {
    char *data = malloc(256);
    snprintf(data, 256, "Temperature: 25.5°C\\nHumidity: 60%%");
    return data; // 调用者负责释放内存
}

// 注册资源
esp_mcp_resource_config_t resource_config = {
    .uri_template = "esp32://sensors/{type}",
    .name = "sensors",
    .title = "Sensor Data",
    .description = "Real-time sensor readings",
    .mime_type = "text/plain",
    .handler = sensor_handler,
    .user_data = NULL
};

esp_mcp_server_register_resource(server, &resource_config);
```

### 统计信息

#### `esp_mcp_server_get_stats()`

获取服务器统计信息。

```c
esp_err_t esp_mcp_server_get_stats(
    esp_mcp_server_handle_t server_handle,
    uint16_t *active_sessions,
    uint16_t *total_tools,
    uint16_t *total_resources
);
```

## 配置选项

### `esp_mcp_server_config_t`

```c
typedef struct {
    uint16_t port;                    // HTTP 服务器端口 (默认: 80)
    uint16_t max_sessions;            // 最大并发会话数 (默认: 10)
    uint32_t session_timeout_ms;      // 会话超时时间 (默认: 300000)
    const char *server_name;          // 服务器名称 (可选)
    const char *server_version;       // 服务器版本 (可选)
} esp_mcp_server_config_t;
```

**默认配置宏:**

```c
esp_mcp_server_config_t config = ESP_MCP_SERVER_DEFAULT_CONFIG();
```

## URI 模板

组件支持参数化的 URI 模板，允许从 URI 中提取参数：

```c
// URI 模板: "echo://{message}"
// 实际 URI: "echo://hello"
// 提取的参数: {"message": "hello"}
```

### `esp_mcp_uri_match_template()`

手动匹配 URI 模板的辅助函数：

```c
bool esp_mcp_uri_match_template(
    const char *template_uri, 
    const char *actual_uri, 
    cJSON **params
);
```

## 回调函数

### 工具处理函数

```c
typedef cJSON* (*esp_mcp_tool_handler_t)(const cJSON *arguments, void *user_data);
```

- **参数:** JSON 格式的工具参数
- **返回:** JSON 格式的执行结果，失败时返回 NULL
- **内存:** 返回的 JSON 对象会被服务器自动释放

### 资源处理函数

```c
typedef char* (*esp_mcp_resource_handler_t)(const char *uri, void *user_data);
```

- **参数:** 请求的资源 URI
- **返回:** 动态分配的字符串内容，失败时返回 NULL
- **内存:** 返回的字符串会被服务器自动释放

## 错误处理

所有 API 函数返回 `esp_err_t` 错误码：

- `ESP_OK`: 成功
- `ESP_ERR_INVALID_ARG`: 无效参数
- `ESP_ERR_NO_MEM`: 内存不足
- `ESP_ERR_INVALID_STATE`: 无效状态（如重复注册）
- `ESP_ERR_NOT_SUPPORTED`: 功能不支持

## 内存管理

- **自动管理**: 服务器自动管理注册的工具和资源的内存
- **复制数据**: 注册函数会复制传入的字符串和 JSON 数据
- **回调返回**: 回调函数返回的内存会被服务器自动释放
- **清理**: 调用 `esp_mcp_server_stop()` 会清理所有相关内存

## 线程安全

当前实现**不是线程安全的**。建议：

- 在主任务中进行所有注册操作
- 避免在多个任务中同时调用 API 函数
- 回调函数应该是线程安全的

## 示例项目

查看 `examples/protocols/mcp_component_example/` 获取完整的使用示例，包括：

- WiFi 连接配置
- 自定义工具注册（echo, GPIO 控制, ADC 读取）
- 自定义资源注册（传感器数据）
- 错误处理和日志记录

## 客户端测试

使用 curl 测试注册的工具：

```bash
curl -X POST http://[ESP32_IP]/mcp \\
  -H "Content-Type: application/json" \\
  -d '{
    "jsonrpc": "2.0",
    "method": "tools/call",
    "params": {
      "name": "echo",
      "arguments": {"message": "Hello World"}
    },
    "id": 1
  }'
```

测试注册的资源：

```bash
curl -X POST http://[ESP32_IP]/mcp \\
  -H "Content-Type: application/json" \\
  -d '{
    "jsonrpc": "2.0",
    "method": "resources/read",
    "params": {
      "uri": "esp32://sensors/temperature"
    },
    "id": 2
  }'
```

## 许可证

本组件遵循 ESP-IDF 的许可证条款。

## 贡献

欢迎提交 Issue 和 Pull Request 来改进这个组件！