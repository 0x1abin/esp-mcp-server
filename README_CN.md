# ESP32 MCP Server 组件

[![Component Registry](https://components.espressif.com/components/0x1abin/esp_mcp_server/badge.svg)](https://components.espressif.com/components/0x1abin/esp_mcp_server)
[![ESP-IDF Version](https://img.shields.io/badge/ESP--IDF-v5.0%2B-blue)](https://github.com/espressif/esp-idf)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)

**中文** | [English](README.md)

一个实现 **模型上下文协议 (MCP)** 服务器的 ESP32 组件，为 AI 应用程序与 ESP32 设备的集成提供标准化方式。该组件使您的 ESP32 能够暴露工具和资源，供 AI 代理和应用程序发现和使用。

## 🌟 特性

- **🚀 简洁 API**: 干净、直观的工具和资源注册接口
- **🔧 动态注册**: 运行时注册工具和资源
- **📦 模块化设计**: 独立组件，易于集成到现有项目
- **🌐 HTTP 传输**: 基于 HTTP 的 JSON-RPC 2.0，最大兼容性
- **🔄 URI 模板**: 支持参数化资源 URI（如 `sensor://{type}/{id}`）
- **📊 实时监控**: 内置服务器统计和健康监控
- **🛡️ 内存安全**: 自动内存管理和清理
- **✅ Schema 验证**: 内置参数验证（类似 Zod）
- **🎯 MCP 兼容**: 完全符合 MCP 规范 2025-06-18

## 📦 安装

### 使用 ESP Component Registry（推荐）

```bash
idf.py add-dependency "0x1abin/esp_mcp_server"
```

### 手动安装

```bash
cd your_project/components
git clone https://github.com/0x1abin/esp-mcp-server.git esp_mcp_server
```

## 🚀 快速开始

```c
#include "esp_mcp_server.h"

// 工具处理函数
static cJSON* echo_tool_handler(const cJSON *arguments, void *user_data) {
    cJSON *message = cJSON_GetObjectItem(arguments, "message");
    
    cJSON *result = cJSON_CreateObject();
    cJSON *content_array = cJSON_CreateArray();
    cJSON *content = cJSON_CreateObject();
    
    cJSON_AddStringToObject(content, "type", "text");
    cJSON_AddStringToObject(content, "text", message->valuestring);
    
    cJSON_AddItemToArray(content_array, content);
    cJSON_AddItemToObject(result, "content", content_array);
    
    return result;
}

void app_main(void) {
    // 初始化 WiFi
    ESP_ERROR_CHECK(example_connect());
    
    // 配置 MCP 服务器
    esp_mcp_server_config_t config = ESP_MCP_SERVER_DEFAULT_CONFIG();
    config.port = 80;
    config.server_name = "ESP32 MCP Server";
    
    // 初始化服务器
    esp_mcp_server_handle_t server;
    ESP_ERROR_CHECK(esp_mcp_server_init(&config, &server));
    
    // 创建输入验证 schema
    cJSON *schema = schema_builder_create_object();
    schema_builder_add_string(schema, "message", "要回显的消息", true);
    
    // 注册工具
    esp_mcp_tool_config_t tool_config = {
        .name = "echo",
        .description = "回显提供的消息",
        .input_schema = schema,
        .handler = echo_tool_handler,
        .user_data = NULL
    };
    ESP_ERROR_CHECK(esp_mcp_server_register_tool(server, &tool_config));
    
    // 启动服务器
    ESP_ERROR_CHECK(esp_mcp_server_start(server));
    
    ESP_LOGI("MAIN", "MCP 服务器已在端口 %d 启动", config.port);
}
```

## 🔧 主要 API

### 服务器生命周期

```c
// 初始化服务器（不启动 HTTP 服务器）
esp_err_t esp_mcp_server_init(const esp_mcp_server_config_t *config, 
                              esp_mcp_server_handle_t *server_handle);

// 注册工具和资源后启动 HTTP 服务器
esp_err_t esp_mcp_server_start(esp_mcp_server_handle_t server_handle);

// 停止 HTTP 服务器（可稍后重启）
esp_err_t esp_mcp_server_stop(esp_mcp_server_handle_t server_handle);

// 清理并释放所有资源
esp_err_t esp_mcp_server_deinit(esp_mcp_server_handle_t server_handle);
```

### 工具注册

```c
// 注册带可选 schema 验证的工具
esp_err_t esp_mcp_server_register_tool(esp_mcp_server_handle_t server_handle, 
                                       const esp_mcp_tool_config_t *tool_config);
```

### Schema 验证（内置类 Zod API）

```c
// 创建 schema 对象
cJSON* schema_builder_create_object(void);

// 添加带验证的类型字段
esp_err_t schema_builder_add_string(cJSON *schema, const char *name, 
                                   const char *description, bool required);
esp_err_t schema_builder_add_integer(cJSON *schema, const char *name, 
                                    const char *description, int min, int max, bool required);
```

## 📊 示例

组件在 `examples/simple/` 中包含完整示例，演示：

- WiFi 连接设置
- MCP 服务器初始化和配置
- 带 schema 验证的工具注册
- 带 URI 模板的资源注册
- GPIO 控制集成
- ADC 读取功能

### 运行示例

```bash
cd examples/simple
idf.py set-target esp32
idf.py menuconfig  # 配置 WiFi 凭据
idf.py build flash monitor
```

## 🧪 测试

使用任何 MCP 兼容客户端测试您的 MCP 服务器：

```bash
# 测试工具列表
curl -X POST http://your-esp32-ip/mcp \
  -H "Content-Type: application/json" \
  -d '{"jsonrpc":"2.0","id":1,"method":"tools/list","params":{}}'

# 调用工具
curl -X POST http://your-esp32-ip/mcp \
  -H "Content-Type: application/json" \
  -d '{"jsonrpc":"2.0","id":2,"method":"tools/call","params":{"name":"echo","arguments":{"message":"你好世界"}}}'
```

## 📄 许可证

本项目采用 MIT 许可证 - 查看 [LICENSE](LICENSE) 文件了解详情。

## 🔗 相关项目

- [模型上下文协议规范](https://modelcontextprotocol.io/)
- [ESP-IDF](https://github.com/espressif/esp-idf)

---

**为 ESP32 和 AI 社区用 ❤️ 制作**
