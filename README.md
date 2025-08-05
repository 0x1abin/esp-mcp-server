# ESP32 MCP Server Component

[![Component Registry](https://components.espressif.com/components/0x1abin/esp_mcp_server/badge.svg)](https://components.espressif.com/components/0x1abin/esp_mcp_server)
[![ESP-IDF Version](https://img.shields.io/badge/ESP--IDF-v5.0%2B-blue)](https://github.com/espressif/esp-idf)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)

[中文文档](README_CN.md) | **English**

An ESP32 component that implements the **Model Context Protocol (MCP)** server, providing a standardized way for AI applications to integrate with ESP32 devices. This component enables your ESP32 to expose tools and resources that can be discovered and used by AI agents and applications.

## 🌟 Features

- **🚀 Simple API**: Clean, intuitive interface for registering tools and resources
- **🔧 Dynamic Registration**: Register tools and resources at runtime
- **📦 Modular Design**: Standalone component, easy to integrate into existing projects
- **🌐 HTTP Transport**: JSON-RPC 2.0 over HTTP for maximum compatibility
- **🔄 URI Templates**: Support for parameterized resource URIs (e.g., `sensor://{type}/{id}`)
- **📊 Real-time Monitoring**: Built-in server statistics and health monitoring
- **🛡️ Memory Safe**: Automatic memory management and cleanup
- **✅ Schema Validation**: Built-in parameter validation using JSON schemas (similar to Zod)
- **🎯 MCP Compliant**: Fully compliant with MCP specification 2025-06-18

## 🏗️ Architecture

The ESP32 MCP Server follows the standard MCP architecture:

```
┌─────────────────┐    HTTP/JSON-RPC    ┌─────────────────┐
│   MCP Client    │ ◄─────────────────► │  ESP32 MCP      │
│  (AI Agent)     │                     │    Server       │
└─────────────────┘                     └─────────────────┘
                                               │
                                        ┌─────────────────┐
                                        │   Your ESP32    │
                                        │   Application   │
                                        │  (Tools & Data) │
                                        └─────────────────┘
```

## 📦 Installation

### Using ESP Component Registry (Recommended)

```bash
idf.py add-dependency "0x1abin/esp_mcp_server"
```

### Manual Installation

1. Clone this repository into your project's `components` directory:
```bash
cd your_project/components
git clone https://github.com/0x1abin/esp-mcp-server.git esp_mcp_server
```

2. Include the component in your main `CMakeLists.txt`:
```cmake
idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS "."
    REQUIRES esp_mcp_server
)
```

## 🚀 Quick Start

### Basic Server Setup

```c
#include "esp_mcp_server.h"

// Tool handler function
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
    // Initialize WiFi (using ESP-IDF examples common)
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    ESP_ERROR_CHECK(example_connect());
    
    // Configure MCP server
    esp_mcp_server_config_t config = ESP_MCP_SERVER_DEFAULT_CONFIG();
    config.port = 80;
    config.server_name = "ESP32 MCP Server";
    config.server_version = "1.0.0";
    
    // Initialize server
    esp_mcp_server_handle_t server;
    ESP_ERROR_CHECK(esp_mcp_server_init(&config, &server));
    
    // Create input schema for validation
    cJSON *schema = schema_builder_create_object();
    schema_builder_add_string(schema, "message", "Message to echo", true);
    
    // Register echo tool
    esp_mcp_tool_config_t tool_config = {
        .name = "echo",
        .description = "Echoes back the provided message",
        .input_schema = schema,
        .handler = echo_tool_handler,
        .user_data = NULL
    };
    ESP_ERROR_CHECK(esp_mcp_server_register_tool(server, &tool_config));
    
    // Start the server
    ESP_ERROR_CHECK(esp_mcp_server_start(server));
    
    ESP_LOGI("MAIN", "MCP Server started on port %d", config.port);
}
```

### Advanced Example with GPIO Control

```c
// GPIO control tool with schema validation
static cJSON* gpio_control_handler(const cJSON *arguments, void *user_data) {
    cJSON *pin = cJSON_GetObjectItem(arguments, "pin");
    cJSON *state = cJSON_GetObjectItem(arguments, "state");
    
    // SDK layer guarantees parameters are validated
    int gpio_num = pin->valueint;
    bool gpio_state = cJSON_IsTrue(state);
    
    gpio_set_level(gpio_num, gpio_state ? 1 : 0);
    
    cJSON *result = cJSON_CreateObject();
    cJSON *content_array = cJSON_CreateArray();
    cJSON *content = cJSON_CreateObject();
    
    cJSON_AddStringToObject(content, "type", "text");
    
    char response[100];
    snprintf(response, sizeof(response), "GPIO %d set to %s", 
             gpio_num, gpio_state ? "HIGH" : "LOW");
    cJSON_AddStringToObject(content, "text", response);
    
    cJSON_AddItemToArray(content_array, content);
    cJSON_AddItemToObject(result, "content", content_array);
    
    return result;
}

// Register GPIO control tool with schema validation
cJSON *gpio_schema = schema_builder_create_object();
schema_builder_add_integer(gpio_schema, "pin", "GPIO pin number", 0, 39, true);
schema_builder_add_boolean(gpio_schema, "state", "GPIO state (true=HIGH, false=LOW)", true);

esp_mcp_tool_config_t gpio_tool = {
    .name = "gpio_control",
    .description = "Control ESP32 GPIO pins",
    .input_schema = gpio_schema,
    .handler = gpio_control_handler,
    .user_data = NULL
};
ESP_ERROR_CHECK(esp_mcp_server_register_tool(server, &gpio_tool));
```

## 🔧 API Reference

### Server Lifecycle

```c
// Initialize server (does not start HTTP server)
esp_err_t esp_mcp_server_init(const esp_mcp_server_config_t *config, 
                              esp_mcp_server_handle_t *server_handle);

// Start HTTP server after registering tools/resources
esp_err_t esp_mcp_server_start(esp_mcp_server_handle_t server_handle);

// Stop HTTP server (can be restarted later)
esp_err_t esp_mcp_server_stop(esp_mcp_server_handle_t server_handle);

// Cleanup and free all resources
esp_err_t esp_mcp_server_deinit(esp_mcp_server_handle_t server_handle);
```

### Tool Registration

```c
// Register a tool with optional schema validation
esp_err_t esp_mcp_server_register_tool(esp_mcp_server_handle_t server_handle, 
                                       const esp_mcp_tool_config_t *tool_config);

// Tool handler signature
typedef cJSON* (*esp_mcp_tool_handler_t)(const cJSON *arguments, void *user_data);
```

### Resource Registration

```c
// Register a resource
esp_err_t esp_mcp_server_register_resource(esp_mcp_server_handle_t server_handle, 
                                           const esp_mcp_resource_config_t *resource_config);

// Resource handler signature
typedef char* (*esp_mcp_resource_handler_t)(const char *uri, void *user_data);
```

### Schema Validation (Built-in Zod-like API)

```c
// Create schema objects
cJSON* schema_builder_create_object(void);

// Add typed fields with validation
esp_err_t schema_builder_add_string(cJSON *schema, const char *name, 
                                   const char *description, bool required);
esp_err_t schema_builder_add_integer(cJSON *schema, const char *name, 
                                    const char *description, int min, int max, bool required);
esp_err_t schema_builder_add_boolean(cJSON *schema, const char *name, 
                                    const char *description, bool required);
```

## 🎯 MCP Protocol Support

This component fully implements the MCP specification:

- **Tools**: Execute actions with side effects (returns JSON)
- **Resources**: Provide data content (returns string/binary)
- **Prompts**: Reusable templates (planned)
- **JSON-RPC 2.0**: Complete protocol compliance
- **Error Handling**: Standard error codes and messages
- **Schema Validation**: Input parameter validation

### Supported MCP Methods

| Method | Description | Status |
|--------|-------------|--------|
| `initialize` | Server initialization and capability negotiation | ✅ |
| `tools/list` | List available tools | ✅ |
| `tools/call` | Execute a tool | ✅ |
| `resources/list` | List available resources | ✅ |
| `resources/read` | Read resource content | ✅ |
| `ping` | Health check | ✅ |

## 📊 Examples

The component includes a comprehensive example in `examples/simple/` that demonstrates:

- WiFi connection setup
- MCP server initialization and configuration
- Tool registration with schema validation
- Resource registration with URI templates
- GPIO control integration
- ADC reading functionality
- Real-time server statistics

### Running the Example

```bash
cd examples/simple
idf.py set-target esp32
idf.py menuconfig  # Configure WiFi credentials
idf.py build flash monitor
```

## 🔧 Configuration Options

```c
typedef struct {
    uint16_t port;                    // HTTP server port (default: 8080)
    const char *server_name;          // Server identification
    const char *server_version;       // Server version
    size_t max_connections;           // Max concurrent connections
    size_t stack_size;                // HTTP server stack size
    int task_priority;                // HTTP server task priority
} esp_mcp_server_config_t;

// Default configuration macro
#define ESP_MCP_SERVER_DEFAULT_CONFIG() { \
    .port = 8080, \
    .server_name = "ESP32 MCP Server", \
    .server_version = "1.0.0", \
    .max_connections = 4, \
    .stack_size = 8192, \
    .task_priority = 5 \
}
```

## 🛡️ Security Considerations

- **Parameter Validation**: All tool inputs are validated against JSON schemas
- **Memory Safety**: Automatic cleanup of JSON objects and strings
- **Error Handling**: Comprehensive error reporting with standard HTTP/JSON-RPC codes
- **Resource Limits**: Configurable connection limits and timeouts

## 🧪 Testing

Test your MCP server using any MCP-compatible client:

```bash
# Using curl to test tools/list
curl -X POST http://your-esp32-ip/mcp \
  -H "Content-Type: application/json" \
  -d '{"jsonrpc":"2.0","id":1,"method":"tools/list","params":{}}'

# Using curl to call a tool
curl -X POST http://your-esp32-ip/mcp \
  -H "Content-Type: application/json" \
  -d '{"jsonrpc":"2.0","id":2,"method":"tools/call","params":{"name":"echo","arguments":{"message":"Hello World"}}}'
```

## 🤝 Contributing

Contributions are welcome! Please feel free to submit a Pull Request. For major changes, please open an issue first to discuss what you would like to change.

## 📄 License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

## 🔗 Related Projects

- [Model Context Protocol Specification](https://modelcontextprotocol.io/)
- [ESP-IDF](https://github.com/espressif/esp-idf)
- [cJSON](https://github.com/DaveGamble/cJSON)

## 📞 Support

- **Issues**: [GitHub Issues](https://github.com/0x1abin/esp-mcp-server/issues)
- **Discussions**: [GitHub Discussions](https://github.com/0x1abin/esp-mcp-server/discussions)
- **ESP32 Forum**: [Espressif Forum](https://esp32.com/)

---

**Made with ❤️ for the ESP32 and AI community**
