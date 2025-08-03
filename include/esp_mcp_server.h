/**
 * @file esp_mcp_server.h
 * @brief ESP32 MCP (Model Context Protocol) Server Component
 *
 * This component provides a high-level API for creating MCP servers on ESP32.
 * It supports dynamic registration of tools and resources with simple callback functions.
 *
 * ## Usage Flow
 *
 * 1. Initialize server: esp_mcp_server_init()
 * 2. Register tools: esp_mcp_server_register_tool()
 * 3. Register resources: esp_mcp_server_register_resource()
 * 4. Start HTTP server: esp_mcp_server_start()
 * 5. Server is now ready to accept MCP requests
 * 6. Stop server: esp_mcp_server_stop() (optional, can restart later)
 * 7. Cleanup: esp_mcp_server_deinit()
 *
 * @code
 * esp_mcp_server_handle_t server;
 * esp_mcp_server_config_t config = ESP_MCP_SERVER_DEFAULT_CONFIG();
 * 
 * // Initialize server
 * ESP_ERROR_CHECK(esp_mcp_server_init(&config, &server));
 * 
 * // Register tools and resources
 * esp_mcp_tool_config_t tool = { ... };
 * ESP_ERROR_CHECK(esp_mcp_server_register_tool(server, &tool));
 * 
 * // Start HTTP server
 * ESP_ERROR_CHECK(esp_mcp_server_start(server));
 * 
 * // Server is now running...
 * 
 * // Cleanup
 * ESP_ERROR_CHECK(esp_mcp_server_deinit(server));
 * @endcode
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "cJSON.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief MCP Server handle type
 */
typedef void* esp_mcp_server_handle_t;

/**
 * @brief Tool execution callback function
 *
 * @param arguments JSON object containing tool arguments
 * @param user_data User data passed during registration
 * @return JSON object containing tool execution result, or NULL on error
 *         The returned JSON object will be automatically freed by the server
 */
typedef cJSON* (*esp_mcp_tool_handler_t)(const cJSON *arguments, void *user_data);

/**
 * @brief Resource read callback function
 *
 * @param uri The resource URI being requested
 * @param user_data User data passed during registration
 * @return Dynamically allocated string containing resource content, or NULL on error
 *         The returned string will be automatically freed by the server
 */
typedef char* (*esp_mcp_resource_handler_t)(const char *uri, void *user_data);

/**
 * @brief Tool configuration structure
 */
typedef struct {
    const char *name;                    ///< Tool name (required)
    const char *title;                   ///< Tool title (optional)
    const char *description;             ///< Tool description (optional)
    cJSON *input_schema;                 ///< JSON schema for input validation (optional)
    esp_mcp_tool_handler_t handler;      ///< Tool execution callback (required)
    void *user_data;                     ///< User data passed to callback (optional)
} esp_mcp_tool_config_t;

/**
 * @brief Resource configuration structure
 */
typedef struct {
    const char *uri_template;            ///< URI template (e.g., "echo://{message}")
    const char *name;                    ///< Resource name (required)
    const char *title;                   ///< Resource title (optional)
    const char *description;             ///< Resource description (optional)
    const char *mime_type;               ///< MIME type (optional, defaults to "text/plain")
    esp_mcp_resource_handler_t handler;  ///< Resource read callback (required)
    void *user_data;                     ///< User data passed to callback (optional)
} esp_mcp_resource_config_t;

/**
 * @brief MCP Server configuration structure
 */
typedef struct {
    uint16_t port;                       ///< HTTP server port (default: 80)
    uint16_t max_sessions;               ///< Maximum concurrent sessions (default: 10)
    uint32_t session_timeout_ms;         ///< Session timeout in milliseconds (default: 300000)
    const char *server_name;             ///< Server name in capabilities (optional)
    const char *server_version;          ///< Server version in capabilities (optional)
} esp_mcp_server_config_t;

/**
 * @brief Default MCP server configuration
 */
#define ESP_MCP_SERVER_DEFAULT_CONFIG() { \
    .port = 80, \
    .max_sessions = 10, \
    .session_timeout_ms = 300000, \
    .server_name = "ESP32 MCP Server", \
    .server_version = "1.0.0" \
}

/**
 * @brief Initialize MCP server (does not start HTTP server)
 *
 * This function initializes the MCP server context and prepares it for tool/resource registration.
 * The HTTP server is not started until esp_mcp_server_start() is called.
 *
 * @param config Server configuration
 * @param server_handle Output handle for the created server
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t esp_mcp_server_init(const esp_mcp_server_config_t *config, esp_mcp_server_handle_t *server_handle);

/**
 * @brief Stop and cleanup MCP server
 *
 * This function stops the HTTP server (if running) and frees all allocated resources.
 * It automatically calls esp_mcp_server_stop() if the server is running.
 *
 * @param server_handle Server handle
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t esp_mcp_server_deinit(esp_mcp_server_handle_t server_handle);

/**
 * @brief Start MCP server HTTP service
 *
 * This function starts the HTTP server and begins accepting MCP requests.
 * The server must be initialized with esp_mcp_server_init() first.
 * Tools and resources should be registered before starting the server.
 *
 * @param server_handle Server handle
 * @return ESP_OK on success, ESP_ERR_INVALID_STATE if already running, error code otherwise
 */
esp_err_t esp_mcp_server_start(esp_mcp_server_handle_t server_handle);

/**
 * @brief Stop MCP server HTTP service
 *
 * This function stops the HTTP server but keeps the server context intact.
 * The server can be restarted with esp_mcp_server_start() after stopping.
 *
 * @param server_handle Server handle
 * @return ESP_OK on success, ESP_ERR_INVALID_STATE if not running, error code otherwise
 */
esp_err_t esp_mcp_server_stop(esp_mcp_server_handle_t server_handle);

/**
 * @brief Register a tool with the MCP server
 *
 * Example usage:
 * @code
 * cJSON* echo_tool_handler(const cJSON *arguments, void *user_data) {
 *     cJSON *message = cJSON_GetObjectItem(arguments, "message");
 *     if (!message || !cJSON_IsString(message)) {
 *         return NULL;
 *     }
 *
 *     cJSON *result = cJSON_CreateObject();
 *     cJSON *content_array = cJSON_CreateArray();
 *     cJSON *content = cJSON_CreateObject();
 *
 *     cJSON_AddStringToObject(content, "type", "text");
 *     char *response = malloc(256);
 *     snprintf(response, 256, "Tool echo: %s", message->valuestring);
 *     cJSON_AddStringToObject(content, "text", response);
 *     free(response);
 *
 *     cJSON_AddItemToArray(content_array, content);
 *     cJSON_AddItemToObject(result, "content", content_array);
 *
 *     return result;
 * }
 *
 * // Create input schema
 * cJSON *schema = cJSON_CreateObject();
 * cJSON_AddStringToObject(schema, "type", "object");
 * cJSON *properties = cJSON_CreateObject();
 * cJSON *message_prop = cJSON_CreateObject();
 * cJSON_AddStringToObject(message_prop, "type", "string");
 * cJSON_AddStringToObject(message_prop, "description", "Message to echo");
 * cJSON_AddItemToObject(properties, "message", message_prop);
 * cJSON_AddItemToObject(schema, "properties", properties);
 *
 * esp_mcp_tool_config_t tool_config = {
 *     .name = "echo",
 *     .title = "Echo Tool",
 *     .description = "Echoes back the provided message",
 *     .input_schema = schema,
 *     .handler = echo_tool_handler,
 *     .user_data = NULL
 * };
 *
 * esp_mcp_server_register_tool(server_handle, &tool_config);
 * cJSON_Delete(schema); // Safe to delete after registration
 * @endcode
 *
 * @param server_handle Server handle
 * @param tool_config Tool configuration
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t esp_mcp_server_register_tool(esp_mcp_server_handle_t server_handle, const esp_mcp_tool_config_t *tool_config);

/**
 * @brief Register a resource with the MCP server
 *
 * Example usage:
 * @code
 * char* echo_resource_handler(const char *uri, void *user_data) {
 *     // Parse URI to extract message parameter
 *     // For simplicity, assume URI is "echo://hello"
 *     const char *message = strstr(uri, "://");
 *     if (message) {
 *         message += 3; // Skip "://"
 *         char *result = malloc(256);
 *         snprintf(result, 256, "Resource echo: %s", message);
 *         return result;
 *     }
 *     return NULL;
 * }
 *
 * esp_mcp_resource_config_t resource_config = {
 *     .uri_template = "echo://{message}",
 *     .name = "echo",
 *     .title = "Echo Resource",
 *     .description = "Echoes back messages as resources",
 *     .mime_type = "text/plain",
 *     .handler = echo_resource_handler,
 *     .user_data = NULL
 * };
 *
 * esp_mcp_server_register_resource(server_handle, &resource_config);
 * @endcode
 *
 * @param server_handle Server handle
 * @param resource_config Resource configuration
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t esp_mcp_server_register_resource(esp_mcp_server_handle_t server_handle, const esp_mcp_resource_config_t *resource_config);

/**
 * @brief Unregister a tool from the MCP server
 *
 * @param server_handle Server handle
 * @param tool_name Tool name to unregister
 * @return ESP_OK on success, ESP_ERR_NOT_FOUND if tool not found
 */
esp_err_t esp_mcp_server_unregister_tool(esp_mcp_server_handle_t server_handle, const char *tool_name);

/**
 * @brief Unregister a resource from the MCP server
 *
 * @param server_handle Server handle
 * @param resource_name Resource name to unregister
 * @return ESP_OK on success, ESP_ERR_NOT_FOUND if resource not found
 */
esp_err_t esp_mcp_server_unregister_resource(esp_mcp_server_handle_t server_handle, const char *resource_name);

/**
 * @brief Get server statistics
 *
 * @param server_handle Server handle
 * @param active_sessions Output for number of active sessions
 * @param total_tools Output for total registered tools
 * @param total_resources Output for total registered resources
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t esp_mcp_server_get_stats(esp_mcp_server_handle_t server_handle,
                                   uint16_t *active_sessions,
                                   uint16_t *total_tools,
                                   uint16_t *total_resources);

#ifdef __cplusplus
}
#endif
