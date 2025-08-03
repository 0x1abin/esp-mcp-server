#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include "cJSON.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief URI template matching helper function
 *
 * This function helps parse URI templates like "echo://{message}" and extract parameters.
 *
 * @param template_uri URI template (e.g., "echo://{message}")
 * @param actual_uri Actual URI (e.g., "echo://hello")
 * @param params Output JSON object containing extracted parameters
 * @return true if URI matches template, false otherwise
 */
bool esp_mcp_uri_match_template(const char *template_uri, const char *actual_uri, cJSON **params);

#ifdef __cplusplus
}
#endif
