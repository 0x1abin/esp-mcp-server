# ESP32 MCP Server Component Example

这是一个展示如何使用 `esp_mcp_server` 组件的完整示例应用。

## 功能特性

### 🔧 自定义工具
- **Echo Tool**: 回显消息工具
- **GPIO Control**: LED 控制工具
- **ADC Read**: ADC 读取工具

### 📦 自定义资源
- **Echo Resource**: 回显资源 (`echo://{message}`)
- **Sensor Data**: 传感器数据资源 (`esp32://sensors/data`)

### 🌐 网络功能
- WiFi Station 模式连接
- HTTP MCP 服务器
- 动态 IP 地址分配

## 硬件要求

- ESP32 开发板
- LED（连接到 GPIO 2，可配置）
- 按钮（连接到 GPIO 0，可配置）
- 可选：模拟传感器连接到 ADC 通道 0

## 快速开始

### 1. 配置 WiFi


### 2. 编译和烧录

```bash
# 设置 ESP-IDF 环境
get_idf

# 编译项目
idf.py build

# 烧录到设备
idf.py flash monitor
```

### 3. 查找 ESP32 IP 地址

从串口监视器中查找类似的日志：
```
I (12345) MCP_EXAMPLE: got ip:192.168.1.100
```

### 4. 测试 MCP 服务器

#### 列出可用工具
```bash
curl -X POST http://192.168.1.100/mcp \\
  -H "Content-Type: application/json" \\
  -d '{
    "jsonrpc": "2.0",
    "method": "tools/list",
    "params": {},
    "id": 1
  }'
```

#### 调用 Echo 工具
```bash
curl -X POST http://192.168.1.100/mcp \\
  -H "Content-Type: application/json" \\
  -d '{
    "jsonrpc": "2.0",
    "method": "tools/call",
    "params": {
      "name": "echo",
      "arguments": {"message": "Hello MCP!"}
    },
    "id": 2
  }'
```

#### 控制 LED
```bash
# 打开 LED
curl -X POST http://192.168.1.100/mcp \\
  -H "Content-Type: application/json" \\
  -d '{
    "jsonrpc": "2.0",
    "method": "tools/call",
    "params": {
      "name": "gpio_control",
      "arguments": {"pin": 2, "state": true}
    },
    "id": 3
  }'

# 关闭 LED
curl -X POST http://192.168.1.100/mcp \\
  -H "Content-Type: application/json" \\
  -d '{
    "jsonrpc": "2.0",
    "method": "tools/call",
    "params": {
      "name": "gpio_control",
      "arguments": {"pin": 2, "state": false}
    },
    "id": 4
  }'
```

#### 读取 ADC 值
```bash
curl -X POST http://192.168.1.100/mcp \\
  -H "Content-Type: application/json" \\
  -d '{
    "jsonrpc": "2.0",
    "method": "tools/call",
    "params": {
      "name": "adc_read",
      "arguments": {}
    },
    "id": 5
  }'
```

#### 列出可用资源
```bash
curl -X POST http://192.168.1.100/mcp \\
  -H "Content-Type: application/json" \\
  -d '{
    "jsonrpc": "2.0",
    "method": "resources/list",
    "params": {},
    "id": 6
  }'
```

#### 读取传感器数据
```bash
curl -X POST http://192.168.1.100/mcp \\
  -H "Content-Type: application/json" \\
  -d '{
    "jsonrpc": "2.0",
    "method": "resources/read",
    "params": {
      "uri": "esp32://sensors/data"
    },
    "id": 7
  }'
```

#### 访问 Echo 资源
```bash
curl -X POST http://192.168.1.100/mcp \\
  -H "Content-Type: application/json" \\
  -d '{
    "jsonrpc": "2.0",
    "method": "resources/read",
    "params": {
      "uri": "echo://hello-world"
    },
    "id": 8
  }'
```

## 代码结构

### 工具实现

每个工具都有一个处理函数，接收 JSON 参数并返回 JSON 结果：

```c
static cJSON* echo_tool_handler(const cJSON *arguments, void *user_data) {
    // 从参数中提取数据
    cJSON *message = cJSON_GetObjectItem(arguments, "message");
    
    // 创建响应
    cJSON *result = cJSON_CreateObject();
    // ... 构建响应内容
    
    return result;
}
```

### 资源实现

每个资源都有一个处理函数，接收 URI 并返回字符串内容：

```c
static char* sensor_data_handler(const char *uri, void *user_data) {
    char *data = malloc(512);
    // ... 生成资源内容
    return data; // 会被服务器自动释放
}
```

### 注册过程

工具和资源在 `register_custom_tools_and_resources()` 函数中注册：

```c
// 创建工具配置
esp_mcp_tool_config_t tool_config = {
    .name = "echo",
    .title = "Echo Tool",
    .description = "Echoes back messages",
    .input_schema = schema,
    .handler = echo_tool_handler,
    .user_data = NULL
};

// 注册工具
esp_mcp_server_register_tool(mcp_server, &tool_config);
```

## 配置选项

### GPIO 配置
- `CONFIG_EXAMPLE_LED_GPIO`: LED GPIO 引脚（默认：2）
- `CONFIG_EXAMPLE_BUTTON_GPIO`: 按钮 GPIO 引脚（默认：0）

### WiFi 配置
- `CONFIG_ESP_WIFI_SSID`: WiFi 网络名称
- `CONFIG_ESP_WIFI_PASSWORD`: WiFi 密码
- `CONFIG_ESP_MAXIMUM_RETRY`: 最大重连次数（默认：5）

## 扩展示例

### 添加新工具

1. 创建处理函数：
```c
static cJSON* my_tool_handler(const cJSON *arguments, void *user_data) {
    // 实现您的工具逻辑
    return result;
}
```

2. 创建输入模式（可选）：
```c
cJSON *schema = cJSON_CreateObject();
// ... 定义参数模式
```

3. 注册工具：
```c
esp_mcp_tool_config_t tool_config = {
    .name = "my_tool",
    .handler = my_tool_handler,
    // ... 其他配置
};
esp_mcp_server_register_tool(mcp_server, &tool_config);
```

### 添加新资源

1. 创建处理函数：
```c
static char* my_resource_handler(const char *uri, void *user_data) {
    char *content = malloc(256);
    // ... 生成资源内容
    return content;
}
```

2. 注册资源：
```c
esp_mcp_resource_config_t resource_config = {
    .uri_template = "my://resource/{param}",
    .name = "my_resource",
    .handler = my_resource_handler,
    // ... 其他配置
};
esp_mcp_server_register_resource(mcp_server, &resource_config);
```

## 故障排除

### 常见问题

1. **WiFi 连接失败**
   - 检查 SSID 和密码配置
   - 确认 WiFi 网络可用
   - 查看串口日志中的错误信息

2. **MCP 服务器启动失败**
   - 检查端口是否被占用
   - 确认内存充足
   - 查看错误日志

3. **工具调用失败**
   - 验证 JSON 格式正确
   - 检查参数类型和名称
   - 查看工具处理函数的实现

### 调试技巧

- 启用详细日志：`CONFIG_LOG_DEFAULT_LEVEL_DEBUG=y`
- 监控内存使用：观察 "Free heap" 日志
- 使用 JSON 格式化工具验证请求格式

## 性能优化

- 减少 JSON 对象的创建和销毁
- 使用静态缓冲区而不是动态分配（适当时）
- 限制并发会话数量
- 优化回调函数的执行时间

## 许可证

本示例遵循 ESP-IDF 的许可证条款。
