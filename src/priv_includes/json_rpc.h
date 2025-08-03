#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "cJSON.h"

#ifdef __cplusplus
extern "C" {
#endif

// JSON-RPC 2.0 Error Codes
#define JSONRPC_PARSE_ERROR     -32700
#define JSONRPC_INVALID_REQUEST -32600
#define JSONRPC_METHOD_NOT_FOUND -32601
#define JSONRPC_INVALID_PARAMS  -32602
#define JSONRPC_INTERNAL_ERROR  -32603

// JSON-RPC Message Types
typedef enum {
    JSONRPC_REQUEST,
    JSONRPC_RESPONSE,
    JSONRPC_NOTIFICATION,
    JSONRPC_ERROR
} jsonrpc_msg_type_t;

// JSON-RPC Message Structure
typedef struct {
    jsonrpc_msg_type_t type;
    char *jsonrpc;      // Must be "2.0"
    char *method;       // For requests and notifications
    cJSON *params;      // Parameters (optional)
    cJSON *id;          // Request ID (null for notifications)
    cJSON *result;      // Response result
    cJSON *error;       // Error object
} jsonrpc_msg_t;

// JSON-RPC Method Handler Function Type
typedef cJSON* (*jsonrpc_method_handler_t)(const cJSON *params, const cJSON *id, void *user_data);

// JSON-RPC Method Registration Structure
typedef struct {
    const char *method;
    jsonrpc_method_handler_t handler;
} jsonrpc_method_t;

/**
 * @brief Parse JSON-RPC message from string
 *
 * @param json_str JSON string to parse
 * @param msg Output message structure
 * @return true if parsing successful, false otherwise
 */
bool jsonrpc_parse_message(const char *json_str, jsonrpc_msg_t *msg);

/**
 * @brief Create JSON-RPC response
 *
 * @param id Request ID
 * @param result Result object (can be NULL)
 * @return JSON string of response (must be freed by caller)
 */
char* jsonrpc_create_response(const cJSON *id, const cJSON *result);

/**
 * @brief Create JSON-RPC error response
 *
 * @param id Request ID
 * @param code Error code
 * @param message Error message
 * @param data Additional error data (optional)
 * @return JSON string of error response (must be freed by caller)
 */
char* jsonrpc_create_error(const cJSON *id, int code, const char *message, const cJSON *data);

/**
 * @brief Create JSON-RPC request
 *
 * @param method Method name
 * @param params Parameters (can be NULL)
 * @param id Request ID
 * @return JSON string of request (must be freed by caller)
 */
char* jsonrpc_create_request(const char *method, const cJSON *params, const cJSON *id);

/**
 * @brief Create JSON-RPC notification
 *
 * @param method Method name
 * @param params Parameters (can be NULL)
 * @return JSON string of notification (must be freed by caller)
 */
char* jsonrpc_create_notification(const char *method, const cJSON *params);

/**
 * @brief Process JSON-RPC message with registered methods
 *
 * @param json_str Input JSON string
 * @param methods Array of registered methods
 * @param method_count Number of registered methods
 * @param user_data User data passed to method handlers
 * @return Response JSON string (must be freed by caller, NULL for notifications)
 */
char* jsonrpc_process_message(const char *json_str, const jsonrpc_method_t *methods, size_t method_count, void *user_data);

/**
 * @brief Process JSON-RPC request (alias for jsonrpc_process_message for compatibility)
 *
 * @param json_str Input JSON string
 * @param methods Array of registered methods
 * @param method_count Number of registered methods
 * @param user_data User data passed to method handlers
 * @return Response JSON string (must be freed by caller, NULL for notifications)
 */
char* jsonrpc_process_request(const char *json_str, const jsonrpc_method_t *methods, size_t method_count, void *user_data);

/**
 * @brief Free JSON-RPC message structure
 *
 * @param msg Message to free
 */
void jsonrpc_free_message(jsonrpc_msg_t *msg);

/**
 * @brief Validate JSON-RPC message structure
 *
 * @param msg Message to validate
 * @return true if valid, false otherwise
 */
bool jsonrpc_validate_message(const jsonrpc_msg_t *msg);

#ifdef __cplusplus
}
#endif
