#include "json_rpc.h"
#include <string.h>
#include <stdlib.h>
#include "esp_log.h"

static const char *TAG = "JSON_RPC";

bool jsonrpc_parse_message(const char *json_str, jsonrpc_msg_t *msg) {
    if (!json_str || !msg) {
        return false;
    }

    // Initialize message structure
    memset(msg, 0, sizeof(jsonrpc_msg_t));

    cJSON *json = cJSON_Parse(json_str);
    if (!json) {
        ESP_LOGE(TAG, "Failed to parse JSON");
        return false;
    }

    // Check for jsonrpc version
    cJSON *jsonrpc = cJSON_GetObjectItem(json, "jsonrpc");
    if (!jsonrpc || !cJSON_IsString(jsonrpc) || strcmp(jsonrpc->valuestring, "2.0") != 0) {
        ESP_LOGE(TAG, "Invalid or missing jsonrpc version");
        cJSON_Delete(json);
        return false;
    }
    msg->jsonrpc = strdup(jsonrpc->valuestring);

    // Get ID (can be null for notifications)
    cJSON *id = cJSON_GetObjectItem(json, "id");
    if (id) {
        msg->id = cJSON_Duplicate(id, 1);
    }

    // Check if it's a request/notification or response
    cJSON *method = cJSON_GetObjectItem(json, "method");
    cJSON *result = cJSON_GetObjectItem(json, "result");
    cJSON *error = cJSON_GetObjectItem(json, "error");

    if (method && cJSON_IsString(method)) {
        // It's a request or notification
        msg->method = strdup(method->valuestring);

        cJSON *params = cJSON_GetObjectItem(json, "params");
        if (params) {
            msg->params = cJSON_Duplicate(params, 1);
        }

        if (msg->id) {
            msg->type = JSONRPC_REQUEST;
        } else {
            msg->type = JSONRPC_NOTIFICATION;
        }
    } else if (result || error) {
        // It's a response
        if (result) {
            msg->result = cJSON_Duplicate(result, 1);
            msg->type = JSONRPC_RESPONSE;
        } else {
            msg->error = cJSON_Duplicate(error, 1);
            msg->type = JSONRPC_ERROR;
        }
    } else {
        ESP_LOGE(TAG, "Invalid JSON-RPC message format");
        cJSON_Delete(json);
        jsonrpc_free_message(msg);
        return false;
    }

    cJSON_Delete(json);
    return true;
}

char* jsonrpc_create_response(const cJSON *id, const cJSON *result) {
    cJSON *response = cJSON_CreateObject();
    if (!response) {
        return NULL;
    }

    cJSON_AddStringToObject(response, "jsonrpc", "2.0");

    if (result) {
        cJSON_AddItemToObject(response, "result", cJSON_Duplicate(result, 1));
    } else {
        cJSON_AddNullToObject(response, "result");
    }

    if (id) {
        cJSON_AddItemToObject(response, "id", cJSON_Duplicate(id, 1));
    } else {
        cJSON_AddNullToObject(response, "id");
    }

    char *response_str = cJSON_Print(response);
    cJSON_Delete(response);
    return response_str;
}

char* jsonrpc_create_error(const cJSON *id, int code, const char *message, const cJSON *data) {
    cJSON *response = cJSON_CreateObject();
    if (!response) {
        return NULL;
    }

    cJSON_AddStringToObject(response, "jsonrpc", "2.0");

    cJSON *error = cJSON_CreateObject();
    cJSON_AddNumberToObject(error, "code", code);
    cJSON_AddStringToObject(error, "message", message ? message : "Unknown error");

    if (data) {
        cJSON_AddItemToObject(error, "data", cJSON_Duplicate(data, 1));
    }

    cJSON_AddItemToObject(response, "error", error);

    if (id) {
        cJSON_AddItemToObject(response, "id", cJSON_Duplicate(id, 1));
    } else {
        cJSON_AddNullToObject(response, "id");
    }

    char *response_str = cJSON_Print(response);
    cJSON_Delete(response);
    return response_str;
}

char* jsonrpc_create_request(const char *method, const cJSON *params, const cJSON *id) {
    if (!method) {
        return NULL;
    }

    cJSON *request = cJSON_CreateObject();
    if (!request) {
        return NULL;
    }

    cJSON_AddStringToObject(request, "jsonrpc", "2.0");
    cJSON_AddStringToObject(request, "method", method);

    if (params) {
        cJSON_AddItemToObject(request, "params", cJSON_Duplicate(params, 1));
    }

    if (id) {
        cJSON_AddItemToObject(request, "id", cJSON_Duplicate(id, 1));
    }

    char *request_str = cJSON_Print(request);
    cJSON_Delete(request);
    return request_str;
}

char* jsonrpc_create_notification(const char *method, const cJSON *params) {
    return jsonrpc_create_request(method, params, NULL);
}

char* jsonrpc_process_message(const char *json_str, const jsonrpc_method_t *methods, size_t method_count, void *user_data) {
    if (!json_str || !methods) {
        return jsonrpc_create_error(NULL, JSONRPC_INVALID_REQUEST, "Invalid parameters", NULL);
    }

    jsonrpc_msg_t msg;
    if (!jsonrpc_parse_message(json_str, &msg)) {
        return jsonrpc_create_error(NULL, JSONRPC_PARSE_ERROR, "Parse error", NULL);
    }

    // Handle only requests and notifications
    if (msg.type != JSONRPC_REQUEST && msg.type != JSONRPC_NOTIFICATION) {
        jsonrpc_free_message(&msg);
        return jsonrpc_create_error(msg.id, JSONRPC_INVALID_REQUEST, "Invalid request", NULL);
    }

    // Find method handler
    jsonrpc_method_handler_t handler = NULL;
    for (size_t i = 0; i < method_count; i++) {
        if (strcmp(methods[i].method, msg.method) == 0) {
            handler = methods[i].handler;
            break;
        }
    }

    if (!handler) {
        char *error_response = NULL;
        if (msg.type == JSONRPC_REQUEST) {
            error_response = jsonrpc_create_error(msg.id, JSONRPC_METHOD_NOT_FOUND, "Method not found", NULL);
        }
        jsonrpc_free_message(&msg);
        return error_response;
    }

    // Call method handler
    cJSON *result = handler(msg.params, msg.id, user_data);
    char *response = NULL;

    if (msg.type == JSONRPC_REQUEST) {
        if (result) {
            // Check if the result is actually an error indicator
            cJSON *error_type = cJSON_GetObjectItem(result, "_jsonrpc_error");
            if (error_type && cJSON_IsString(error_type)) {
                // Handle special error types
                int error_code = JSONRPC_INTERNAL_ERROR;
                const char *default_message = "Internal error";

                if (strcmp(error_type->valuestring, "invalid_params") == 0) {
                    error_code = JSONRPC_INVALID_PARAMS;
                    default_message = "Invalid params";
                }

                cJSON *message = cJSON_GetObjectItem(result, "message");
                cJSON *data = cJSON_GetObjectItem(result, "data");

                response = jsonrpc_create_error(msg.id, error_code,
                    message && cJSON_IsString(message) ? message->valuestring : default_message,
                    data);
            } else {
                response = jsonrpc_create_response(msg.id, result);
            }
            cJSON_Delete(result);
        } else {
            response = jsonrpc_create_error(msg.id, JSONRPC_INTERNAL_ERROR, "Internal error", NULL);
        }
    }
    // Notifications don't return responses

    jsonrpc_free_message(&msg);
    return response;
}

char* jsonrpc_process_request(const char *json_str, const jsonrpc_method_t *methods, size_t method_count, void *user_data) {
    return jsonrpc_process_message(json_str, methods, method_count, user_data);
}

void jsonrpc_free_message(jsonrpc_msg_t *msg) {
    if (!msg) {
        return;
    }

    if (msg->jsonrpc) {
        free(msg->jsonrpc);
        msg->jsonrpc = NULL;
    }

    if (msg->method) {
        free(msg->method);
        msg->method = NULL;
    }

    if (msg->params) {
        cJSON_Delete(msg->params);
        msg->params = NULL;
    }

    if (msg->id) {
        cJSON_Delete(msg->id);
        msg->id = NULL;
    }

    if (msg->result) {
        cJSON_Delete(msg->result);
        msg->result = NULL;
    }

    if (msg->error) {
        cJSON_Delete(msg->error);
        msg->error = NULL;
    }
}

bool jsonrpc_validate_message(const jsonrpc_msg_t *msg) {
    if (!msg || !msg->jsonrpc) {
        return false;
    }

    if (strcmp(msg->jsonrpc, "2.0") != 0) {
        return false;
    }

    switch (msg->type) {
        case JSONRPC_REQUEST:
            return msg->method && msg->id;
        case JSONRPC_NOTIFICATION:
            return msg->method && !msg->id;
        case JSONRPC_RESPONSE:
            return msg->result && msg->id;
        case JSONRPC_ERROR:
            return msg->error && msg->id;
        default:
            return false;
    }
}
