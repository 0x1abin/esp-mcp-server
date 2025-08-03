/**
 * @file main.c
 * @brief ESP32 MCP Server Component Example Application
 *
 * This example demonstrates how to use the esp_mcp_server component
 * to create a custom MCP server with registered tools and resources.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_timer.h"
#include "driver/gpio.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_mcp_server.h"

#include "protocol_examples_common.h"

static const char *TAG = "MCP_EXAMPLE";

// GPIO configuration
#define EXAMPLE_LED_GPIO           CONFIG_EXAMPLE_LED_GPIO
#define EXAMPLE_BUTTON_GPIO        CONFIG_EXAMPLE_BUTTON_GPIO

// ADC configuration
#define EXAMPLE_ADC_UNIT           ADC_UNIT_1
#define EXAMPLE_ADC_CHANNEL        ADC_CHANNEL_0
#define EXAMPLE_ADC_ATTEN          ADC_ATTEN_DB_12

static esp_mcp_server_handle_t mcp_server = NULL;
static adc_oneshot_unit_handle_t adc_handle = NULL;
static adc_cali_handle_t adc_cali_handle = NULL;

// Initialize hardware
static void hardware_init(void) {
    // Configure LED GPIO
    gpio_config_t led_config = {
        .pin_bit_mask = (1ULL << EXAMPLE_LED_GPIO),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&led_config);
    gpio_set_level(EXAMPLE_LED_GPIO, 0);

    // Configure button GPIO
    gpio_config_t button_config = {
        .pin_bit_mask = (1ULL << EXAMPLE_BUTTON_GPIO),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&button_config);

    // Initialize ADC
    adc_oneshot_unit_init_cfg_t init_config = {
        .unit_id = EXAMPLE_ADC_UNIT,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config, &adc_handle));

    adc_oneshot_chan_cfg_t config = {
        .bitwidth = ADC_BITWIDTH_DEFAULT,
        .atten = EXAMPLE_ADC_ATTEN,
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc_handle, EXAMPLE_ADC_CHANNEL, &config));

    // Initialize ADC calibration
    adc_cali_curve_fitting_config_t cali_config = {
        .unit_id = EXAMPLE_ADC_UNIT,
        .chan = EXAMPLE_ADC_CHANNEL,
        .atten = EXAMPLE_ADC_ATTEN,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    esp_err_t ret = adc_cali_create_scheme_curve_fitting(&cali_config, &adc_cali_handle);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "ADC calibration not supported, raw values will be used");
        adc_cali_handle = NULL;
    }

    ESP_LOGI(TAG, "Hardware initialized");
}

// Custom tool handlers

/**
 * @brief Echo tool handler - echoes back the provided message
 */
static cJSON* echo_tool_handler(const cJSON *arguments, void *user_data) {
    ESP_LOGI(TAG, "Echo tool called");

    cJSON *message = cJSON_GetObjectItem(arguments, "message");
    if (!message || !cJSON_IsString(message)) {
        return NULL;
    }

    cJSON *result = cJSON_CreateObject();
    cJSON *content_array = cJSON_CreateArray();
    cJSON *content = cJSON_CreateObject();

    cJSON_AddStringToObject(content, "type", "text");

    char *response = malloc(256);
    snprintf(response, 256, "Tool echo: %s", message->valuestring);
    cJSON_AddStringToObject(content, "text", response);
    free(response);

    cJSON_AddItemToArray(content_array, content);
    cJSON_AddItemToObject(result, "content", content_array);

    return result;
}

/**
 * @brief GPIO control tool handler - controls LED GPIO
 */
static cJSON* gpio_control_handler(const cJSON *arguments, void *user_data) {
    ESP_LOGI(TAG, "GPIO control tool called");

    cJSON *pin = cJSON_GetObjectItem(arguments, "pin");
    cJSON *state = cJSON_GetObjectItem(arguments, "state");

    cJSON *result = cJSON_CreateObject();
    cJSON *content_array = cJSON_CreateArray();
    cJSON *content = cJSON_CreateObject();
    cJSON_AddStringToObject(content, "type", "text");

    if (pin && cJSON_IsNumber(pin) && state && cJSON_IsBool(state)) {
        int gpio_num = pin->valueint;
        bool gpio_state = cJSON_IsTrue(state);

        if (gpio_num == EXAMPLE_LED_GPIO) {
            gpio_set_level(gpio_num, gpio_state ? 1 : 0);

            char response[100];
            snprintf(response, sizeof(response), "GPIO %d set to %s",
                    gpio_num, gpio_state ? "HIGH" : "LOW");
            cJSON_AddStringToObject(content, "text", response);
        } else {
            cJSON_AddStringToObject(content, "text", "Invalid GPIO pin. Only LED GPIO is supported.");
        }
    } else {
        cJSON_AddStringToObject(content, "text", "Invalid arguments. Expected: pin (number), state (boolean)");
    }

    cJSON_AddItemToArray(content_array, content);
    cJSON_AddItemToObject(result, "content", content_array);

    return result;
}

/**
 * @brief ADC read tool handler - reads ADC value
 */
static cJSON* adc_read_handler(const cJSON *arguments, void *user_data) {
    ESP_LOGI(TAG, "ADC read tool called");

    cJSON *result = cJSON_CreateObject();
    cJSON *content_array = cJSON_CreateArray();
    cJSON *content = cJSON_CreateObject();
    cJSON_AddStringToObject(content, "type", "text");

    int adc_raw = 0;
    esp_err_t ret = adc_oneshot_read(adc_handle, EXAMPLE_ADC_CHANNEL, &adc_raw);

    if (ret == ESP_OK) {
        char response[200];
        if (adc_cali_handle) {
            int voltage = 0;
            adc_cali_raw_to_voltage(adc_cali_handle, adc_raw, &voltage);
            snprintf(response, sizeof(response),
                    "ADC Channel %d: Raw=%d, Voltage=%dmV",
                    EXAMPLE_ADC_CHANNEL, adc_raw, voltage);
        } else {
            snprintf(response, sizeof(response),
                    "ADC Channel %d: Raw=%d (calibration not available)",
                    EXAMPLE_ADC_CHANNEL, adc_raw);
        }
        cJSON_AddStringToObject(content, "text", response);
    } else {
        cJSON_AddStringToObject(content, "text", "Failed to read ADC");
    }

    cJSON_AddItemToArray(content_array, content);
    cJSON_AddItemToObject(result, "content", content_array);

    return result;
}

// Custom resource handlers

/**
 * @brief Echo resource handler - echoes back messages as resources
 */
static char* echo_resource_handler(const char *uri, void *user_data) {
    ESP_LOGI(TAG, "Echo resource accessed: %s", uri);

    // Parse URI to extract message parameter
    // For simplicity, assume URI is "echo://message"
    const char *message = strstr(uri, "://");
    if (message) {
        message += 3; // Skip "://"
        char *result = malloc(256);
        snprintf(result, 256, "Resource echo: %s", message);
        return result;
    }

    return NULL;
}

/**
 * @brief Sensor data resource handler - returns current sensor readings
 */
static char* sensor_data_handler(const char *uri, void *user_data) {
    ESP_LOGI(TAG, "Sensor data resource accessed: %s", uri);

    char *data = malloc(512);
    if (!data) {
        return NULL;
    }

    // Read current sensor values
    int adc_raw = 0;
    bool button_state = gpio_get_level(EXAMPLE_BUTTON_GPIO) == 0; // Assuming active low
    int voltage = 0;

    adc_oneshot_read(adc_handle, EXAMPLE_ADC_CHANNEL, &adc_raw);
    if (adc_cali_handle) {
        adc_cali_raw_to_voltage(adc_cali_handle, adc_raw, &voltage);
    }

    uint32_t timestamp = esp_timer_get_time() / 1000000; // seconds

    snprintf(data, 512,
            "Sensor Data Report\n"
            "==================\n"
            "Timestamp: %" PRIu32 " seconds\n"
            "ADC Raw: %d\n"
            "ADC Voltage: %dmV\n"
            "Button State: %s\n"
            "Free Heap: %" PRIu32 " bytes\n"
            "Status: Active\n",
            timestamp, adc_raw, voltage,
            button_state ? "PRESSED" : "RELEASED",
            esp_get_free_heap_size());

    return data;
}

// Register custom tools and resources
static void register_custom_tools_and_resources(void) {
    esp_err_t ret;

    // Register echo tool
    cJSON *echo_schema = cJSON_CreateObject();
    cJSON_AddStringToObject(echo_schema, "type", "object");
    cJSON *echo_properties = cJSON_CreateObject();
    cJSON *message_prop = cJSON_CreateObject();
    cJSON_AddStringToObject(message_prop, "type", "string");
    cJSON_AddStringToObject(message_prop, "description", "Message to echo");
    cJSON_AddItemToObject(echo_properties, "message", message_prop);
    cJSON_AddItemToObject(echo_schema, "properties", echo_properties);
    cJSON *echo_required = cJSON_CreateArray();
    cJSON_AddItemToArray(echo_required, cJSON_CreateString("message"));
    cJSON_AddItemToObject(echo_schema, "required", echo_required);

    esp_mcp_tool_config_t echo_tool = {
        .name = "echo",
        .title = "Echo Tool",
        .description = "Echoes back the provided message",
        .input_schema = echo_schema,
        .handler = echo_tool_handler,
        .user_data = NULL
    };

    ret = esp_mcp_server_register_tool(mcp_server, &echo_tool);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register echo tool: %s", esp_err_to_name(ret));
    }
    cJSON_Delete(echo_schema);

    // Register GPIO control tool
    cJSON *gpio_schema = cJSON_CreateObject();
    cJSON_AddStringToObject(gpio_schema, "type", "object");
    cJSON *gpio_properties = cJSON_CreateObject();

    cJSON *pin_prop = cJSON_CreateObject();
    cJSON_AddStringToObject(pin_prop, "type", "integer");
    cJSON_AddStringToObject(pin_prop, "description", "GPIO pin number");
    cJSON_AddItemToObject(gpio_properties, "pin", pin_prop);

    cJSON *state_prop = cJSON_CreateObject();
    cJSON_AddStringToObject(state_prop, "type", "boolean");
    cJSON_AddStringToObject(state_prop, "description", "GPIO state (true=HIGH, false=LOW)");
    cJSON_AddItemToObject(gpio_properties, "state", state_prop);

    cJSON_AddItemToObject(gpio_schema, "properties", gpio_properties);
    cJSON *gpio_required = cJSON_CreateArray();
    cJSON_AddItemToArray(gpio_required, cJSON_CreateString("pin"));
    cJSON_AddItemToArray(gpio_required, cJSON_CreateString("state"));
    cJSON_AddItemToObject(gpio_schema, "required", gpio_required);

    esp_mcp_tool_config_t gpio_tool = {
        .name = "gpio_control",
        .title = "GPIO Control",
        .description = "Control GPIO pins on ESP32",
        .input_schema = gpio_schema,
        .handler = gpio_control_handler,
        .user_data = NULL
    };

    ret = esp_mcp_server_register_tool(mcp_server, &gpio_tool);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register GPIO control tool: %s", esp_err_to_name(ret));
    }
    cJSON_Delete(gpio_schema);

    // Register ADC read tool
    cJSON *adc_schema = cJSON_CreateObject();
    cJSON_AddStringToObject(adc_schema, "type", "object");
    cJSON_AddItemToObject(adc_schema, "properties", cJSON_CreateObject());

    esp_mcp_tool_config_t adc_tool = {
        .name = "adc_read",
        .title = "ADC Read",
        .description = "Read ADC channel value",
        .input_schema = adc_schema,
        .handler = adc_read_handler,
        .user_data = NULL
    };

    ret = esp_mcp_server_register_tool(mcp_server, &adc_tool);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register ADC read tool: %s", esp_err_to_name(ret));
    }
    cJSON_Delete(adc_schema);

    // Register echo resource
    esp_mcp_resource_config_t echo_resource = {
        .uri_template = "echo://{message}",
        .name = "echo",
        .title = "Echo Resource",
        .description = "Echoes back messages as resources",
        .mime_type = "text/plain",
        .handler = echo_resource_handler,
        .user_data = NULL
    };

    ret = esp_mcp_server_register_resource(mcp_server, &echo_resource);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register echo resource: %s", esp_err_to_name(ret));
    }

    // Register sensor data resource
    esp_mcp_resource_config_t sensor_resource = {
        .uri_template = "esp32://sensors/data",
        .name = "sensor_data",
        .title = "Sensor Data",
        .description = "Current sensor readings from ESP32",
        .mime_type = "text/plain",
        .handler = sensor_data_handler,
        .user_data = NULL
    };

    ret = esp_mcp_server_register_resource(mcp_server, &sensor_resource);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register sensor data resource: %s", esp_err_to_name(ret));
    }
}

void app_main(void) {
    ESP_LOGI(TAG, "Starting MCP Server Component Example");

    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // Initialize hardware
    hardware_init();

    /* This helper function configures Wi-Fi or Ethernet, as selected in menuconfig.
     * Read "Establishing Wi-Fi or Ethernet Connection" section in
     * examples/protocols/README.md for more information about this function.
     */
    ESP_ERROR_CHECK(example_connect());

    // Configure and initialize MCP server
    esp_mcp_server_config_t server_config = ESP_MCP_SERVER_DEFAULT_CONFIG();
    server_config.port = 80;
    server_config.server_name = "ESP32 Component Example";
    server_config.server_version = "1.0.0";

    ESP_ERROR_CHECK(esp_mcp_server_init(&server_config, &mcp_server));

    // Register custom tools and resources
    register_custom_tools_and_resources();

    // Start the MCP server
    ESP_ERROR_CHECK(esp_mcp_server_start(mcp_server));

    // Print server statistics
    uint16_t active_sessions, total_tools, total_resources;
    esp_mcp_server_get_stats(mcp_server, &active_sessions, &total_tools, &total_resources);
    ESP_LOGI(TAG, "MCP Server Statistics:");
    ESP_LOGI(TAG, "  Active Sessions: %d", active_sessions);
    ESP_LOGI(TAG, "  Total Tools: %d", total_tools);
    ESP_LOGI(TAG, "  Total Resources: %d", total_resources);

    ESP_LOGI(TAG, "MCP Server Component Example started successfully!");
    ESP_LOGI(TAG, "Free heap: %" PRIu32 " bytes", esp_get_free_heap_size());

    // Main loop
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(10000));
        ESP_LOGI(TAG, "MCP server running... Free heap: %" PRIu32 " bytes", esp_get_free_heap_size());
    }
}
