/**
 * @file schema_validator.c
 * @brief Lightweight JSON Schema Validator Implementation
 */

#include "schema_validator.h"
#include <string.h>
#include <limits.h>
#include "esp_log.h"

static const char *TAG = "SCHEMA_VALIDATOR";

// Forward declarations
static esp_err_t validate_object(const cJSON *data, const cJSON *schema, schema_validation_result_t *result, const char *path);
static esp_err_t validate_property(const cJSON *data, const cJSON *schema, schema_validation_result_t *result, const char *path);

/**
 * @brief Set validation error
 */
static void set_validation_error(schema_validation_result_t *result, schema_validation_error_t error, const char *message, const char *path) {
    result->error = error;
    strncpy(result->error_message, message, sizeof(result->error_message) - 1);
    result->error_message[sizeof(result->error_message) - 1] = '\0';
    result->error_path = path;
}

/**
 * @brief Validate a single property against its schema
 */
static esp_err_t validate_property(const cJSON *data, const cJSON *schema, schema_validation_result_t *result, const char *path) {
    // Get the type from schema
    cJSON *type_item = cJSON_GetObjectItem(schema, "type");
    if (!type_item || !cJSON_IsString(type_item)) {
        set_validation_error(result, SCHEMA_VALIDATION_INVALID_SCHEMA, "Schema missing or invalid type", path);
        return ESP_FAIL;
    }

    const char *expected_type = type_item->valuestring;

    // Validate based on type
    if (strcmp(expected_type, "string") == 0) {
        if (!cJSON_IsString(data)) {
            set_validation_error(result, SCHEMA_VALIDATION_TYPE_MISMATCH, "Expected string", path);
            return ESP_FAIL;
        }
    } else if (strcmp(expected_type, "integer") == 0 || strcmp(expected_type, "number") == 0) {
        if (!cJSON_IsNumber(data)) {
            set_validation_error(result, SCHEMA_VALIDATION_TYPE_MISMATCH, "Expected number", path);
            return ESP_FAIL;
        }

        // Check range if specified
        cJSON *minimum = cJSON_GetObjectItem(schema, "minimum");
        cJSON *maximum = cJSON_GetObjectItem(schema, "maximum");

        double value = data->valuedouble;
        if (minimum && cJSON_IsNumber(minimum) && value < minimum->valuedouble) {
            set_validation_error(result, SCHEMA_VALIDATION_OUT_OF_RANGE, "Value below minimum", path);
            return ESP_FAIL;
        }
        if (maximum && cJSON_IsNumber(maximum) && value > maximum->valuedouble) {
            set_validation_error(result, SCHEMA_VALIDATION_OUT_OF_RANGE, "Value above maximum", path);
            return ESP_FAIL;
        }
    } else if (strcmp(expected_type, "boolean") == 0) {
        if (!cJSON_IsBool(data)) {
            set_validation_error(result, SCHEMA_VALIDATION_TYPE_MISMATCH, "Expected boolean", path);
            return ESP_FAIL;
        }
    } else if (strcmp(expected_type, "object") == 0) {
        if (!cJSON_IsObject(data)) {
            set_validation_error(result, SCHEMA_VALIDATION_TYPE_MISMATCH, "Expected object", path);
            return ESP_FAIL;
        }
        return validate_object(data, schema, result, path);
    } else {
        set_validation_error(result, SCHEMA_VALIDATION_INVALID_SCHEMA, "Unsupported type in schema", path);
        return ESP_FAIL;
    }

    return ESP_OK;
}

/**
 * @brief Validate an object against its schema
 */
static esp_err_t validate_object(const cJSON *data, const cJSON *schema, schema_validation_result_t *result, const char *path) {
    // Get properties and required fields from schema
    cJSON *properties = cJSON_GetObjectItem(schema, "properties");
    cJSON *required = cJSON_GetObjectItem(schema, "required");

    if (!properties || !cJSON_IsObject(properties)) {
        // No properties defined, accept any object
        return ESP_OK;
    }

    // Check required fields
    if (required && cJSON_IsArray(required)) {
        cJSON *required_item = NULL;
        cJSON_ArrayForEach(required_item, required) {
            if (cJSON_IsString(required_item)) {
                const char *field_name = required_item->valuestring;
                if (!cJSON_GetObjectItem(data, field_name)) {
                    char error_msg[128];
                    snprintf(error_msg, sizeof(error_msg), "Missing required field: %s", field_name);
                    set_validation_error(result, SCHEMA_VALIDATION_MISSING_REQUIRED, error_msg, path);
                    return ESP_FAIL;
                }
            }
        }
    }

    // Validate each property in the data
    cJSON *data_item = NULL;
    cJSON_ArrayForEach(data_item, data) {
        const char *property_name = data_item->string;
        cJSON *property_schema = cJSON_GetObjectItem(properties, property_name);

        if (!property_schema) {
            // Property not defined in schema - could be strict or lenient
            // For now, we'll be lenient and allow extra properties
            continue;
        }

        // Create path for this property
        char property_path[256];
        snprintf(property_path, sizeof(property_path), "%s.%s", path, property_name);

        esp_err_t ret = validate_property(data_item, property_schema, result, property_path);
        if (ret != ESP_OK) {
            return ret;
        }
    }

    return ESP_OK;
}

esp_err_t schema_validate(const cJSON *data, const cJSON *schema, schema_validation_result_t *result) {
    if (!data || !schema || !result) {
        return ESP_ERR_INVALID_ARG;
    }

    // Initialize result
    memset(result, 0, sizeof(schema_validation_result_t));
    result->error = SCHEMA_VALIDATION_OK;

    return validate_property(data, schema, result, "root");
}

esp_err_t schema_validate_tool_arguments(const cJSON *arguments, const cJSON *input_schema, schema_validation_result_t *result) {
    // If no schema provided, accept any arguments
    if (!input_schema) {
        if (result) {
            memset(result, 0, sizeof(schema_validation_result_t));
            result->error = SCHEMA_VALIDATION_OK;
        }
        return ESP_OK;
    }

    // If no arguments provided, check if schema requires any
    if (!arguments) {
        cJSON *empty_obj = cJSON_CreateObject();
        esp_err_t ret = schema_validate(empty_obj, input_schema, result);
        cJSON_Delete(empty_obj);
        return ret;
    }

    return schema_validate(arguments, input_schema, result);
}

// Schema creation helpers
cJSON* schema_create_string(const char *description, bool required) {
    cJSON *schema = cJSON_CreateObject();
    cJSON_AddStringToObject(schema, "type", "string");
    if (description) {
        cJSON_AddStringToObject(schema, "description", description);
    }
    return schema;
}

cJSON* schema_create_integer(const char *description, int32_t min_value, int32_t max_value, bool required) {
    cJSON *schema = cJSON_CreateObject();
    cJSON_AddStringToObject(schema, "type", "integer");
    if (description) {
        cJSON_AddStringToObject(schema, "description", description);
    }
    if (min_value != INT32_MIN) {
        cJSON_AddNumberToObject(schema, "minimum", min_value);
    }
    if (max_value != INT32_MAX) {
        cJSON_AddNumberToObject(schema, "maximum", max_value);
    }
    return schema;
}

cJSON* schema_create_boolean(const char *description, bool required) {
    cJSON *schema = cJSON_CreateObject();
    cJSON_AddStringToObject(schema, "type", "boolean");
    if (description) {
        cJSON_AddStringToObject(schema, "description", description);
    }
    return schema;
}

cJSON* schema_create_object(cJSON *properties, const char **required_fields) {
    cJSON *schema = cJSON_CreateObject();
    cJSON_AddStringToObject(schema, "type", "object");

    if (properties) {
        cJSON_AddItemToObject(schema, "properties", properties);
    }

    if (required_fields) {
        cJSON *required_array = cJSON_CreateArray();
        for (int i = 0; required_fields[i] != NULL; i++) {
            cJSON_AddItemToArray(required_array, cJSON_CreateString(required_fields[i]));
        }
        cJSON_AddItemToObject(schema, "required", required_array);
    }

    return schema;
}

// Schema builder helpers
cJSON* schema_builder_create_object(void) {
    cJSON *schema = cJSON_CreateObject();
    cJSON_AddStringToObject(schema, "type", "object");
    cJSON_AddItemToObject(schema, "properties", cJSON_CreateObject());
    cJSON_AddItemToObject(schema, "required", cJSON_CreateArray());
    return schema;
}

esp_err_t schema_builder_add_string(cJSON *schema_obj, const char *property_name, const char *description, bool required) {
    if (!schema_obj || !property_name) {
        return ESP_ERR_INVALID_ARG;
    }

    cJSON *properties = cJSON_GetObjectItem(schema_obj, "properties");
    if (!properties) {
        return ESP_ERR_INVALID_STATE;
    }

    cJSON *property_schema = schema_create_string(description, required);
    cJSON_AddItemToObject(properties, property_name, property_schema);

    if (required) {
        cJSON *required_array = cJSON_GetObjectItem(schema_obj, "required");
        if (required_array && cJSON_IsArray(required_array)) {
            cJSON_AddItemToArray(required_array, cJSON_CreateString(property_name));
        }
    }

    return ESP_OK;
}

esp_err_t schema_builder_add_integer(cJSON *schema_obj, const char *property_name, const char *description, int32_t min_value, int32_t max_value, bool required) {
    if (!schema_obj || !property_name) {
        return ESP_ERR_INVALID_ARG;
    }

    cJSON *properties = cJSON_GetObjectItem(schema_obj, "properties");
    if (!properties) {
        return ESP_ERR_INVALID_STATE;
    }

    cJSON *property_schema = schema_create_integer(description, min_value, max_value, required);
    cJSON_AddItemToObject(properties, property_name, property_schema);

    if (required) {
        cJSON *required_array = cJSON_GetObjectItem(schema_obj, "required");
        if (required_array && cJSON_IsArray(required_array)) {
            cJSON_AddItemToArray(required_array, cJSON_CreateString(property_name));
        }
    }

    return ESP_OK;
}

esp_err_t schema_builder_add_boolean(cJSON *schema_obj, const char *property_name, const char *description, bool required) {
    if (!schema_obj || !property_name) {
        return ESP_ERR_INVALID_ARG;
    }

    cJSON *properties = cJSON_GetObjectItem(schema_obj, "properties");
    if (!properties) {
        return ESP_ERR_INVALID_STATE;
    }

    cJSON *property_schema = schema_create_boolean(description, required);
    cJSON_AddItemToObject(properties, property_name, property_schema);

    if (required) {
        cJSON *required_array = cJSON_GetObjectItem(schema_obj, "required");
        if (required_array && cJSON_IsArray(required_array)) {
            cJSON_AddItemToArray(required_array, cJSON_CreateString(property_name));
        }
    }

    return ESP_OK;
}
