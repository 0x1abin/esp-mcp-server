/**
 * @file esp_mcp_server.c
 * @brief ESP32 MCP Server Component - Unified Implementation
 */

#include <string.h>
#include <stdlib.h>
#include <inttypes.h>
#include "esp_log.h"
#include "esp_http_server.h"
#include "esp_timer.h"
#include "esp_chip_info.h"
#include "esp_system.h"
#include "esp_idf_version.h"
#include "cJSON.h"
#include "json_rpc.h"
#include "uri_template.h"
#include "esp_mcp_server.h"

static const char *TAG = "ESP_MCP_SERVER";

// Internal server context structure
typedef struct {
    httpd_handle_t http_server;
    esp_mcp_server_config_t config;

    // Registered tools and resources
    struct {
        char *name;
        char *title;
        char *description;
        cJSON *input_schema;
        esp_mcp_tool_handler_t handler;
        void *user_data;
    } *tools;
    size_t tool_count;
    size_t tool_capacity;

    struct {
        char *uri_template;
        char *name;
        char *title;
        char *description;
        char *mime_type;
        esp_mcp_resource_handler_t handler;
        void *user_data;
    } *resources;
    size_t resource_count;
    size_t resource_capacity;

    // Session management
    uint16_t active_sessions;
} mcp_server_ctx_t;

// Forward declarations for MCP protocol handlers
static cJSON* handle_initialize(const cJSON *params, const cJSON *id, void *user_data);
static cJSON* handle_initialized(const cJSON *params, const cJSON *id, void *user_data);
static cJSON* handle_ping(const cJSON *params, const cJSON *id, void *user_data);
static cJSON* handle_list_tools(const cJSON *params, const cJSON *id, void *user_data);
static cJSON* handle_call_tool(const cJSON *params, const cJSON *id, void *user_data);
static cJSON* handle_list_resources(const cJSON *params, const cJSON *id, void *user_data);
static cJSON* handle_read_resource(const cJSON *params, const cJSON *id, void *user_data);

// JSON-RPC method table
static const jsonrpc_method_t mcp_methods[] = {
    {"initialize", handle_initialize},
    {"initialized", handle_initialized},
    {"ping", handle_ping},
    {"tools/list", handle_list_tools},
    {"tools/call", handle_call_tool},
    {"resources/list", handle_list_resources},
    {"resources/read", handle_read_resource},
};

static const size_t mcp_methods_count = sizeof(mcp_methods) / sizeof(mcp_methods[0]);

// Built-in system info tool
static cJSON* builtin_system_info_tool(const cJSON *arguments, void *user_data) {
    cJSON *result = cJSON_CreateObject();
    if (!result) {
        return NULL;
    }

    cJSON *content_array = cJSON_CreateArray();
    cJSON *content = cJSON_CreateObject();
    cJSON_AddStringToObject(content, "type", "text");

    char info_text[512];
    snprintf(info_text, sizeof(info_text),
            "ESP32 System Information:\n"
            "- Free heap: %" PRIu32 " bytes\n"
            "- Minimum free heap: %" PRIu32 " bytes\n"
            "- Uptime: %" PRId64 " ms\n"
            "- IDF Version: %s\n",
            esp_get_free_heap_size(),
            esp_get_minimum_free_heap_size(),
            esp_timer_get_time() / 1000,
            esp_get_idf_version());

    cJSON_AddStringToObject(content, "text", info_text);
    cJSON_AddItemToArray(content_array, content);
    cJSON_AddItemToObject(result, "content", content_array);

    return result;
}

// Built-in system status resource
static char* builtin_system_status_resource(const char *uri, void *user_data) {
    char *status_text = malloc(1024);
    if (!status_text) {
        return NULL;
    }

    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);
    mcp_server_ctx_t *ctx = (mcp_server_ctx_t *)user_data;

    snprintf(status_text, 1024,
            "ESP32 System Status Report\n"
            "==========================\n"
            "Free Heap: %" PRIu32 " bytes\n"
            "Min Free Heap: %" PRIu32 " bytes\n"
            "Uptime: %" PRId64 " ms\n"
            "IDF Version: %s\n"
            "Active Sessions: %d\n"
            "Chip Model: %s\n"
            "Chip Revision: %d\n",
            esp_get_free_heap_size(),
            esp_get_minimum_free_heap_size(),
            esp_timer_get_time() / 1000,
            esp_get_idf_version(),
            ctx ? ctx->active_sessions : 0,
            CONFIG_IDF_TARGET,
            chip_info.revision);

    return status_text;
}

// MCP protocol handlers implementation
static cJSON* handle_initialize(const cJSON *params, const cJSON *id, void *user_data) {
    ESP_LOGI(TAG, "Initialize request");

    cJSON *result = cJSON_CreateObject();
    if (!result) {
        return NULL;
    }

    // Server capabilities
    cJSON *capabilities = cJSON_CreateObject();
    cJSON *tools = cJSON_CreateObject();
    cJSON_AddBoolToObject(tools, "listChanged", false);
    cJSON_AddItemToObject(capabilities, "tools", tools);

    cJSON *resources = cJSON_CreateObject();
    cJSON_AddBoolToObject(resources, "subscribe", false);
    cJSON_AddBoolToObject(resources, "listChanged", false);
    cJSON_AddItemToObject(capabilities, "resources", resources);

    cJSON_AddItemToObject(result, "capabilities", capabilities);

    // Server info
    mcp_server_ctx_t *ctx = (mcp_server_ctx_t *)user_data;
    cJSON *server_info = cJSON_CreateObject();
    cJSON_AddStringToObject(server_info, "name", ctx ? ctx->config.server_name : "ESP32 MCP Server");
    cJSON_AddStringToObject(server_info, "version", ctx ? ctx->config.server_version : "1.0.0");
    cJSON_AddItemToObject(result, "serverInfo", server_info);

    // Protocol version
    cJSON_AddStringToObject(result, "protocolVersion", "2025-06-18");

    return result;
}

static cJSON* handle_initialized(const cJSON *params, const cJSON *id, void *user_data) {
    ESP_LOGI(TAG, "Initialized notification");
    return NULL; // Notifications don't return responses
}

static cJSON* handle_ping(const cJSON *params, const cJSON *id, void *user_data) {
    ESP_LOGI(TAG, "Ping request");

    cJSON *result = cJSON_CreateObject();
    if (result) {
        cJSON_AddStringToObject(result, "status", "pong");
    }
    return result;
}

static cJSON* handle_list_tools(const cJSON *params, const cJSON *id, void *user_data) {
    ESP_LOGI(TAG, "Listing tools");

    mcp_server_ctx_t *ctx = (mcp_server_ctx_t *)user_data;
    cJSON *result = cJSON_CreateObject();
    if (!result) {
        return NULL;
    }

    cJSON *tools_array = cJSON_CreateArray();

    // Add registered tools
    if (ctx) {
        for (size_t i = 0; i < ctx->tool_count; i++) {
            cJSON *tool = cJSON_CreateObject();
            cJSON_AddStringToObject(tool, "name", ctx->tools[i].name);
            if (ctx->tools[i].title) {
                cJSON_AddStringToObject(tool, "title", ctx->tools[i].title);
            }
            if (ctx->tools[i].description) {
                cJSON_AddStringToObject(tool, "description", ctx->tools[i].description);
            }
            if (ctx->tools[i].input_schema) {
                cJSON_AddItemToObject(tool, "inputSchema", cJSON_Duplicate(ctx->tools[i].input_schema, 1));
            }
            cJSON_AddItemToArray(tools_array, tool);
        }
    }

    // Add built-in system info tool if no custom tools registered
    if (cJSON_GetArraySize(tools_array) == 0) {
        cJSON *tool = cJSON_CreateObject();
        cJSON_AddStringToObject(tool, "name", "get_system_info");
        cJSON_AddStringToObject(tool, "title", "System Information");
        cJSON_AddStringToObject(tool, "description", "Get ESP32 system information");

        cJSON *input_schema = cJSON_CreateObject();
        cJSON_AddStringToObject(input_schema, "type", "object");
        cJSON_AddItemToObject(input_schema, "properties", cJSON_CreateObject());
        cJSON_AddItemToObject(tool, "inputSchema", input_schema);

        cJSON_AddItemToArray(tools_array, tool);
    }

    cJSON_AddItemToObject(result, "tools", tools_array);
    return result;
}

static cJSON* handle_call_tool(const cJSON *params, const cJSON *id, void *user_data) {
    ESP_LOGI(TAG, "Tool call request");

    if (!params) {
        return NULL;
    }

    cJSON *name = cJSON_GetObjectItem(params, "name");
    if (!name || !cJSON_IsString(name)) {
        return NULL;
    }

    cJSON *arguments = cJSON_GetObjectItem(params, "arguments");
    mcp_server_ctx_t *ctx = (mcp_server_ctx_t *)user_data;

    // First, try registered tools
    if (ctx) {
        for (size_t i = 0; i < ctx->tool_count; i++) {
            if (strcmp(ctx->tools[i].name, name->valuestring) == 0) {
                if (ctx->tools[i].handler) {
                    return ctx->tools[i].handler(arguments, ctx->tools[i].user_data);
                }
            }
        }
    }

    // Fallback to built-in tools
    if (strcmp(name->valuestring, "get_system_info") == 0) {
        return builtin_system_info_tool(arguments, user_data);
    }

    // Tool not found
    cJSON *result = cJSON_CreateObject();
    if (result) {
        cJSON_AddStringToObject(result, "error", "Unknown tool");
    }
    return result;
}

static cJSON* handle_list_resources(const cJSON *params, const cJSON *id, void *user_data) {
    ESP_LOGI(TAG, "Listing resources");

    mcp_server_ctx_t *ctx = (mcp_server_ctx_t *)user_data;
    cJSON *result = cJSON_CreateObject();
    if (!result) {
        return NULL;
    }

    cJSON *resources_array = cJSON_CreateArray();

    // Add registered resources
    if (ctx) {
        for (size_t i = 0; i < ctx->resource_count; i++) {
            cJSON *resource = cJSON_CreateObject();
            cJSON_AddStringToObject(resource, "uri", ctx->resources[i].uri_template);
            cJSON_AddStringToObject(resource, "name", ctx->resources[i].name);
            if (ctx->resources[i].title) {
                cJSON_AddStringToObject(resource, "title", ctx->resources[i].title);
            }
            if (ctx->resources[i].description) {
                cJSON_AddStringToObject(resource, "description", ctx->resources[i].description);
            }
            if (ctx->resources[i].mime_type) {
                cJSON_AddStringToObject(resource, "mimeType", ctx->resources[i].mime_type);
            }
            cJSON_AddItemToArray(resources_array, resource);
        }
    }

    // Add built-in system status resource if no custom resources registered
    if (cJSON_GetArraySize(resources_array) == 0) {
        cJSON *resource = cJSON_CreateObject();
        cJSON_AddStringToObject(resource, "uri", "esp32://system/status");
        cJSON_AddStringToObject(resource, "name", "system_status");
        cJSON_AddStringToObject(resource, "title", "System Status");
        cJSON_AddStringToObject(resource, "description", "Current ESP32 system status");
        cJSON_AddStringToObject(resource, "mimeType", "text/plain");

        cJSON_AddItemToArray(resources_array, resource);
    }

    cJSON_AddItemToObject(result, "resources", resources_array);
    return result;
}

static cJSON* handle_read_resource(const cJSON *params, const cJSON *id, void *user_data) {
    ESP_LOGI(TAG, "Reading resource");

    if (!params) {
        return NULL;
    }

    cJSON *uri = cJSON_GetObjectItem(params, "uri");
    if (!uri || !cJSON_IsString(uri)) {
        return NULL;
    }

    mcp_server_ctx_t *ctx = (mcp_server_ctx_t *)user_data;

    // First, try registered resources
    if (ctx) {
        for (size_t i = 0; i < ctx->resource_count; i++) {
            cJSON *params_obj = NULL;
            if (esp_mcp_uri_match_template(ctx->resources[i].uri_template, uri->valuestring, &params_obj)) {
                if (ctx->resources[i].handler) {
                    char *content_text = ctx->resources[i].handler(uri->valuestring, ctx->resources[i].user_data);
                    if (content_text) {
                        cJSON *result = cJSON_CreateObject();
                        if (result) {
                            cJSON *contents_array = cJSON_CreateArray();
                            cJSON *content = cJSON_CreateObject();

                            cJSON_AddStringToObject(content, "uri", uri->valuestring);
                            cJSON_AddStringToObject(content, "mimeType",
                                ctx->resources[i].mime_type ? ctx->resources[i].mime_type : "text/plain");
                            cJSON_AddStringToObject(content, "text", content_text);

                            cJSON_AddItemToArray(contents_array, content);
                            cJSON_AddItemToObject(result, "contents", contents_array);

                            free(content_text);
                            if (params_obj) cJSON_Delete(params_obj);
                            return result;
                        }
                        free(content_text);
                    }
                }
                if (params_obj) cJSON_Delete(params_obj);
            }
        }
    }

    // Fallback to built-in resources
    if (strcmp(uri->valuestring, "esp32://system/status") == 0) {
        char *content_text = builtin_system_status_resource(uri->valuestring, user_data);
        if (content_text) {
            cJSON *result = cJSON_CreateObject();
            if (result) {
                cJSON *contents_array = cJSON_CreateArray();
                cJSON *content = cJSON_CreateObject();

                cJSON_AddStringToObject(content, "uri", uri->valuestring);
                cJSON_AddStringToObject(content, "mimeType", "text/plain");
                cJSON_AddStringToObject(content, "text", content_text);

                cJSON_AddItemToArray(contents_array, content);
                cJSON_AddItemToObject(result, "contents", contents_array);

                free(content_text);
                return result;
            }
            free(content_text);
        }
    }

    // Resource not found
    cJSON *result = cJSON_CreateObject();
    if (result) {
        cJSON_AddStringToObject(result, "error", "Resource not found");
    }
    return result;
}

// HTTP handlers
static esp_err_t mcp_post_handler(httpd_req_t *req) {
    mcp_server_ctx_t *ctx = (mcp_server_ctx_t *)req->user_ctx;

    // Set CORS headers
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Methods", "POST, GET, OPTIONS");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Headers", "Content-Type, MCP-Protocol-Version");

    char *content = malloc(req->content_len + 1);
    if (!content) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Memory allocation failed");
        return ESP_ERR_NO_MEM;
    }

    int ret = httpd_req_recv(req, content, req->content_len);
    if (ret <= 0) {
        free(content);
        if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
            httpd_resp_send_err(req, HTTPD_408_REQ_TIMEOUT, "Request timeout");
        }
        return ESP_FAIL;
    }
    content[req->content_len] = '\0';

    ESP_LOGI(TAG, "Received MCP request: %s", content);

    // Parse and validate JSON-RPC message
    jsonrpc_msg_t msg;
    bool parse_success = jsonrpc_parse_message(content, &msg);

    if (!parse_success) {
        free(content);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON-RPC request");
        return ESP_FAIL;
    }

    // Process JSON-RPC request
    char *response = jsonrpc_process_request(content, mcp_methods, mcp_methods_count, ctx);
    free(content);

    if (response) {
        // We have a response (for requests)
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, response, strlen(response));
        free(response);
    } else {
        // No response (for notifications) - send empty 200 OK
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, NULL, 0);
    }

    jsonrpc_free_message(&msg);
    return ESP_OK;
}

static esp_err_t mcp_options_handler(httpd_req_t *req) {
    // Handle CORS preflight
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Methods", "POST, GET, OPTIONS");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Headers", "Content-Type, MCP-Protocol-Version");
    httpd_resp_send(req, NULL, 0);
    return ESP_OK;
}

// Helper functions for resource management
static esp_err_t expand_tool_array(mcp_server_ctx_t *ctx) {
    if (ctx->tool_count >= ctx->tool_capacity) {
        size_t new_capacity = ctx->tool_capacity * 2;
        void *new_tools = realloc(ctx->tools, new_capacity * sizeof(ctx->tools[0]));
        if (!new_tools) {
            return ESP_ERR_NO_MEM;
        }
        ctx->tools = new_tools;
        ctx->tool_capacity = new_capacity;
    }
    return ESP_OK;
}

static esp_err_t expand_resource_array(mcp_server_ctx_t *ctx) {
    if (ctx->resource_count >= ctx->resource_capacity) {
        size_t new_capacity = ctx->resource_capacity * 2;
        void *new_resources = realloc(ctx->resources, new_capacity * sizeof(ctx->resources[0]));
        if (!new_resources) {
            return ESP_ERR_NO_MEM;
        }
        ctx->resources = new_resources;
        ctx->resource_capacity = new_capacity;
    }
    return ESP_OK;
}

// Public API implementations
esp_err_t esp_mcp_server_start(const esp_mcp_server_config_t *config, esp_mcp_server_handle_t *server_handle) {
    if (!config || !server_handle) {
        ESP_LOGE(TAG, "Invalid arguments");
        return ESP_ERR_INVALID_ARG;
    }

    // Validate configuration
    if (config->port == 0 || config->max_sessions == 0) {
        ESP_LOGE(TAG, "Invalid configuration: port and max_sessions must be > 0");
        return ESP_ERR_INVALID_ARG;
    }

    mcp_server_ctx_t *ctx = calloc(1, sizeof(mcp_server_ctx_t));
    if (!ctx) {
        return ESP_ERR_NO_MEM;
    }

    // Copy configuration
    ctx->config = *config;
    if (config->server_name) {
        ctx->config.server_name = strdup(config->server_name);
    }
    if (config->server_version) {
        ctx->config.server_version = strdup(config->server_version);
    }

    // Initialize arrays
    ctx->tool_capacity = 8;
    ctx->tools = calloc(ctx->tool_capacity, sizeof(ctx->tools[0]));
    if (!ctx->tools) {
        free(ctx);
        return ESP_ERR_NO_MEM;
    }

    ctx->resource_capacity = 8;
    ctx->resources = calloc(ctx->resource_capacity, sizeof(ctx->resources[0]));
    if (!ctx->resources) {
        free(ctx->tools);
        free(ctx);
        return ESP_ERR_NO_MEM;
    }

    // Start HTTP server
    httpd_config_t server_config = HTTPD_DEFAULT_CONFIG();
    server_config.server_port = config->port;
    server_config.max_uri_handlers = 8;

    esp_err_t ret = httpd_start(&ctx->http_server, &server_config);
    if (ret != ESP_OK) {
        free(ctx->resources);
        free(ctx->tools);
        free(ctx);
        ESP_LOGE(TAG, "Failed to start HTTP server: %s", esp_err_to_name(ret));
        return ret;
    }

    // Register URI handlers
    httpd_uri_t mcp_post_uri = {
        .uri = "/mcp",
        .method = HTTP_POST,
        .handler = mcp_post_handler,
        .user_ctx = ctx
    };
    httpd_register_uri_handler(ctx->http_server, &mcp_post_uri);

    httpd_uri_t mcp_options_uri = {
        .uri = "/mcp",
        .method = HTTP_OPTIONS,
        .handler = mcp_options_handler,
        .user_ctx = ctx
    };
    httpd_register_uri_handler(ctx->http_server, &mcp_options_uri);

    *server_handle = (esp_mcp_server_handle_t)ctx;
    ESP_LOGI(TAG, "MCP Server started successfully on port %d", config->port);
    return ESP_OK;
}

esp_err_t esp_mcp_server_stop(esp_mcp_server_handle_t server_handle) {
    if (!server_handle) {
        ESP_LOGE(TAG, "Invalid server handle");
        return ESP_ERR_INVALID_ARG;
    }

    mcp_server_ctx_t *ctx = (mcp_server_ctx_t *)server_handle;

    // Stop HTTP server
    if (ctx->http_server) {
        esp_err_t ret = httpd_stop(ctx->http_server);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "Failed to stop HTTP server: %s", esp_err_to_name(ret));
        }
    }

    // Cleanup tools
    for (size_t i = 0; i < ctx->tool_count; i++) {
        free(ctx->tools[i].name);
        free(ctx->tools[i].title);
        free(ctx->tools[i].description);
        if (ctx->tools[i].input_schema) {
            cJSON_Delete(ctx->tools[i].input_schema);
        }
    }
    free(ctx->tools);

    // Cleanup resources
    for (size_t i = 0; i < ctx->resource_count; i++) {
        free(ctx->resources[i].uri_template);
        free(ctx->resources[i].name);
        free(ctx->resources[i].title);
        free(ctx->resources[i].description);
        free(ctx->resources[i].mime_type);
    }
    free(ctx->resources);

    // Cleanup config strings
    if (ctx->config.server_name) {
        free((char*)ctx->config.server_name);
    }
    if (ctx->config.server_version) {
        free((char*)ctx->config.server_version);
    }

    free(ctx);
    ESP_LOGI(TAG, "MCP Server stopped successfully");
    return ESP_OK;
}

esp_err_t esp_mcp_server_register_tool(esp_mcp_server_handle_t server_handle, const esp_mcp_tool_config_t *tool_config) {
    if (!server_handle || !tool_config) {
        ESP_LOGE(TAG, "Invalid arguments");
        return ESP_ERR_INVALID_ARG;
    }

    if (!tool_config->name || !tool_config->handler) {
        ESP_LOGE(TAG, "Tool name and handler are required");
        return ESP_ERR_INVALID_ARG;
    }

    mcp_server_ctx_t *ctx = (mcp_server_ctx_t *)server_handle;

    // Check if tool already exists
    for (size_t i = 0; i < ctx->tool_count; i++) {
        if (strcmp(ctx->tools[i].name, tool_config->name) == 0) {
            ESP_LOGE(TAG, "Tool '%s' already registered", tool_config->name);
            return ESP_ERR_INVALID_STATE;
        }
    }

    // Expand array if needed
    esp_err_t ret = expand_tool_array(ctx);
    if (ret != ESP_OK) {
        return ret;
    }

    // Add new tool
    size_t idx = ctx->tool_count;
    ctx->tools[idx].name = strdup(tool_config->name);
    ctx->tools[idx].title = tool_config->title ? strdup(tool_config->title) : NULL;
    ctx->tools[idx].description = tool_config->description ? strdup(tool_config->description) : NULL;
    ctx->tools[idx].input_schema = tool_config->input_schema ? cJSON_Duplicate(tool_config->input_schema, 1) : NULL;
    ctx->tools[idx].handler = tool_config->handler;
    ctx->tools[idx].user_data = tool_config->user_data;

    if (!ctx->tools[idx].name) {
        return ESP_ERR_NO_MEM;
    }

    ctx->tool_count++;
    ESP_LOGI(TAG, "Tool '%s' registered successfully", tool_config->name);
    return ESP_OK;
}

esp_err_t esp_mcp_server_register_resource(esp_mcp_server_handle_t server_handle, const esp_mcp_resource_config_t *resource_config) {
    if (!server_handle || !resource_config) {
        ESP_LOGE(TAG, "Invalid arguments");
        return ESP_ERR_INVALID_ARG;
    }

    if (!resource_config->uri_template || !resource_config->name || !resource_config->handler) {
        ESP_LOGE(TAG, "Resource URI template, name, and handler are required");
        return ESP_ERR_INVALID_ARG;
    }

    mcp_server_ctx_t *ctx = (mcp_server_ctx_t *)server_handle;

    // Check if resource already exists
    for (size_t i = 0; i < ctx->resource_count; i++) {
        if (strcmp(ctx->resources[i].name, resource_config->name) == 0) {
            ESP_LOGE(TAG, "Resource '%s' already registered", resource_config->name);
            return ESP_ERR_INVALID_STATE;
        }
    }

    // Expand array if needed
    esp_err_t ret = expand_resource_array(ctx);
    if (ret != ESP_OK) {
        return ret;
    }

    // Add new resource
    size_t idx = ctx->resource_count;
    ctx->resources[idx].uri_template = strdup(resource_config->uri_template);
    ctx->resources[idx].name = strdup(resource_config->name);
    ctx->resources[idx].title = resource_config->title ? strdup(resource_config->title) : NULL;
    ctx->resources[idx].description = resource_config->description ? strdup(resource_config->description) : NULL;
    ctx->resources[idx].mime_type = resource_config->mime_type ? strdup(resource_config->mime_type) : NULL;
    ctx->resources[idx].handler = resource_config->handler;
    ctx->resources[idx].user_data = resource_config->user_data;

    if (!ctx->resources[idx].uri_template || !ctx->resources[idx].name) {
        return ESP_ERR_NO_MEM;
    }

    ctx->resource_count++;
    ESP_LOGI(TAG, "Resource '%s' registered successfully", resource_config->name);
    return ESP_OK;
}

esp_err_t esp_mcp_server_unregister_tool(esp_mcp_server_handle_t server_handle, const char *tool_name) {
    if (!server_handle || !tool_name) {
        ESP_LOGE(TAG, "Invalid arguments");
        return ESP_ERR_INVALID_ARG;
    }

    // TODO: Implement tool unregistration
    ESP_LOGW(TAG, "Tool unregistration not yet implemented");
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t esp_mcp_server_unregister_resource(esp_mcp_server_handle_t server_handle, const char *resource_name) {
    if (!server_handle || !resource_name) {
        ESP_LOGE(TAG, "Invalid arguments");
        return ESP_ERR_INVALID_ARG;
    }

    // TODO: Implement resource unregistration
    ESP_LOGW(TAG, "Resource unregistration not yet implemented");
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t esp_mcp_server_get_stats(esp_mcp_server_handle_t server_handle,
                                   uint16_t *active_sessions,
                                   uint16_t *total_tools,
                                   uint16_t *total_resources) {
    if (!server_handle) {
        ESP_LOGE(TAG, "Invalid server handle");
        return ESP_ERR_INVALID_ARG;
    }

    mcp_server_ctx_t *ctx = (mcp_server_ctx_t *)server_handle;

    if (active_sessions) *active_sessions = ctx->active_sessions;
    if (total_tools) *total_tools = ctx->tool_count;
    if (total_resources) *total_resources = ctx->resource_count;

    return ESP_OK;
}
