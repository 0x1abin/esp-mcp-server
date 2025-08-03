/**
 * @file uri_template.c
 * @brief URI template matching implementation
 */

#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include <ctype.h>
#include "esp_log.h"
#include "cJSON.h"

static const char *TAG = "URI_TEMPLATE";

/**
 * @brief Extract parameter name from template segment
 *
 * @param segment Template segment like "{message}"
 * @param param_name Output buffer for parameter name
 * @param param_name_len Buffer length
 * @return true if parameter extracted successfully
 */
static bool extract_param_name(const char *segment, char *param_name, size_t param_name_len) {
    if (!segment || !param_name || param_name_len == 0) {
        return false;
    }

    // Check if segment is a parameter (starts with { and ends with })
    size_t len = strlen(segment);
    if (len < 3 || segment[0] != '{' || segment[len-1] != '}') {
        return false;
    }

    // Extract parameter name (without { and })
    size_t param_len = len - 2;
    if (param_len >= param_name_len) {
        return false;
    }

    strncpy(param_name, segment + 1, param_len);
    param_name[param_len] = '\0';

    return true;
}

/**
 * @brief Split URI into segments
 *
 * @param uri URI to split
 * @param segments Output array of segment pointers
 * @param max_segments Maximum number of segments
 * @return Number of segments found
 */
static int split_uri_segments(const char *uri, char **segments, int max_segments) {
    if (!uri || !segments || max_segments <= 0) {
        return 0;
    }

    char *uri_copy = strdup(uri);
    if (!uri_copy) {
        return 0;
    }

    int count = 0;
    char *token = strtok(uri_copy, "/");

    while (token && count < max_segments) {
        segments[count] = strdup(token);
        if (!segments[count]) {
            // Cleanup on failure
            for (int i = 0; i < count; i++) {
                free(segments[i]);
            }
            free(uri_copy);
            return 0;
        }
        count++;
        token = strtok(NULL, "/");
    }

    free(uri_copy);
    return count;
}

bool esp_mcp_uri_match_template(const char *template_uri, const char *actual_uri, cJSON **params) {
    if (!template_uri || !actual_uri || !params) {
        return false;
    }

    *params = NULL;
    bool match = false;  // Initialize match variable early

    // Split both URIs into segments
    char *template_segments[16];
    char *actual_segments[16];

    int template_count = split_uri_segments(template_uri, template_segments, 16);
    int actual_count = split_uri_segments(actual_uri, actual_segments, 16);

    if (template_count != actual_count || template_count == 0) {
        goto cleanup;
    }

    // Create parameters object
    cJSON *param_obj = cJSON_CreateObject();
    if (!param_obj) {
        goto cleanup;
    }

    match = true;  // Set to true only when we have valid segments and param_obj

    // Compare segments
    for (int i = 0; i < template_count; i++) {
        char param_name[64];

        if (extract_param_name(template_segments[i], param_name, sizeof(param_name))) {
            // This is a parameter segment, extract value
            cJSON_AddStringToObject(param_obj, param_name, actual_segments[i]);
            ESP_LOGD(TAG, "Extracted parameter: %s = %s", param_name, actual_segments[i]);
        } else {
            // This is a literal segment, must match exactly
            if (strcmp(template_segments[i], actual_segments[i]) != 0) {
                ESP_LOGD(TAG, "Segment mismatch: '%s' != '%s'", template_segments[i], actual_segments[i]);
                match = false;
                break;
            }
        }
    }

    if (match) {
        *params = param_obj;
    } else {
        cJSON_Delete(param_obj);
    }

cleanup:
    // Free segment arrays
    for (int i = 0; i < template_count; i++) {
        free(template_segments[i]);
    }
    for (int i = 0; i < actual_count; i++) {
        free(actual_segments[i]);
    }

    return match && (*params != NULL);
}
