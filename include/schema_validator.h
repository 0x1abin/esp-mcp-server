/**
 * @file schema_validator.h
 * @brief Lightweight JSON Schema Validator for ESP32 MCP Server
 * 
 * This module provides basic JSON schema validation functionality
 * optimized for ESP32 environment, similar to TypeScript's Zod.
 */

#pragma once

#include <stdbool.h>
#include "cJSON.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Schema validation error codes
 */
typedef enum {
    SCHEMA_VALIDATION_OK = 0,
    SCHEMA_VALIDATION_TYPE_MISMATCH,
    SCHEMA_VALIDATION_MISSING_REQUIRED,
    SCHEMA_VALIDATION_INVALID_FORMAT,
    SCHEMA_VALIDATION_OUT_OF_RANGE,
    SCHEMA_VALIDATION_UNKNOWN_PROPERTY,
    SCHEMA_VALIDATION_INVALID_SCHEMA
} schema_validation_error_t;

/**
 * @brief Schema validation result
 */
typedef struct {
    schema_validation_error_t error;
    char error_message[128];
    const char *error_path;
} schema_validation_result_t;

/**
 * @brief Validate JSON data against a JSON schema
 * 
 * @param data JSON data to validate
 * @param schema JSON schema for validation
 * @param result Validation result (output)
 * @return ESP_OK if validation passes, ESP_FAIL if validation fails
 */
esp_err_t schema_validate(const cJSON *data, const cJSON *schema, schema_validation_result_t *result);

/**
 * @brief Validate tool arguments against input schema
 * 
 * This is a convenience function specifically for MCP tool validation
 * 
 * @param arguments Tool arguments JSON
 * @param input_schema Tool input schema JSON
 * @param result Validation result (output)
 * @return ESP_OK if validation passes, ESP_FAIL if validation fails
 */
esp_err_t schema_validate_tool_arguments(const cJSON *arguments, const cJSON *input_schema, schema_validation_result_t *result);

/**
 * @brief Create a simple string schema
 * 
 * @param description Optional description
 * @param required Whether the field is required
 * @return cJSON schema object (must be freed by caller)
 */
cJSON* schema_create_string(const char *description, bool required);

/**
 * @brief Create a simple integer schema
 * 
 * @param description Optional description
 * @param min_value Minimum value (or INT32_MIN for no limit)
 * @param max_value Maximum value (or INT32_MAX for no limit)
 * @param required Whether the field is required
 * @return cJSON schema object (must be freed by caller)
 */
cJSON* schema_create_integer(const char *description, int32_t min_value, int32_t max_value, bool required);

/**
 * @brief Create a simple boolean schema
 * 
 * @param description Optional description
 * @param required Whether the field is required
 * @return cJSON schema object (must be freed by caller)
 */
cJSON* schema_create_boolean(const char *description, bool required);

/**
 * @brief Create an object schema with properties
 * 
 * @param properties cJSON object containing property schemas
 * @param required_fields Array of required field names (NULL terminated)
 * @return cJSON schema object (must be freed by caller)
 */
cJSON* schema_create_object(cJSON *properties, const char **required_fields);

/**
 * @brief Schema builder helper - start building an object schema
 * 
 * @return New schema builder object
 */
cJSON* schema_builder_create_object(void);

/**
 * @brief Schema builder helper - add string property
 * 
 * @param schema_obj Schema object being built
 * @param property_name Property name
 * @param description Optional description
 * @param required Whether this property is required
 * @return ESP_OK on success
 */
esp_err_t schema_builder_add_string(cJSON *schema_obj, const char *property_name, const char *description, bool required);

/**
 * @brief Schema builder helper - add integer property
 * 
 * @param schema_obj Schema object being built
 * @param property_name Property name
 * @param description Optional description
 * @param min_value Minimum value
 * @param max_value Maximum value
 * @param required Whether this property is required
 * @return ESP_OK on success
 */
esp_err_t schema_builder_add_integer(cJSON *schema_obj, const char *property_name, const char *description, int32_t min_value, int32_t max_value, bool required);

/**
 * @brief Schema builder helper - add boolean property
 * 
 * @param schema_obj Schema object being built
 * @param property_name Property name
 * @param description Optional description
 * @param required Whether this property is required
 * @return ESP_OK on success
 */
esp_err_t schema_builder_add_boolean(cJSON *schema_obj, const char *property_name, const char *description, bool required);

#ifdef __cplusplus
}
#endif