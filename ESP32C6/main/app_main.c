#include <inttypes.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "driver/gpio.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "sdkconfig.h"

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_system.h>

#include "protocol_examples_common.h"

#include "esp_crt_bundle.h"
#include "mqtt_client.h"

#ifndef APP_CPU_NUM
#define APP_CPU_NUM PRO_CPU_NUM
#endif

static const char *TAG = "mqtts_example";

static esp_mqtt_client_handle_t mqtt_client = NULL;
static volatile bool mqtt_connected = false;

#if CONFIG_EXAMPLE_BROKER_CERTIFICATE_OVERRIDDEN
static const char cert_override_pem[] =
    "-----BEGIN CERTIFICATE-----\n"
    CONFIG_EXAMPLE_BROKER_CERTIFICATE_OVERRIDE "\n"
    "-----END CERTIFICATE-----";
#endif

#if CONFIG_EXAMPLE_CERT_VALIDATE_MOSQUITTO_CA
/* Embedded Mosquitto CA certificate for test.mosquitto.org:8883 */
extern const uint8_t mosquitto_org_crt_start[] asm("_binary_mosquitto_org_crt_start");
extern const uint8_t mosquitto_org_crt_end[] asm("_binary_mosquitto_org_crt_end");
#endif

/*
 * @brief Event handler registered to receive MQTT events
 *
 *  This function is called by the MQTT client event loop.
 *
 * @param handler_args user data registered to the event.
 * @param base Event base for the handler(always MQTT Base in this example).
 * @param event_id The id for the received event.
 * @param event_data The data for the event, esp_mqtt_event_handle_t.
 */
static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    ESP_LOGD(TAG, "Event dispatched from event loop base=%s, event_id=%" PRIi32, base, event_id);
    esp_mqtt_event_handle_t event = event_data;
    esp_mqtt_client_handle_t client = event->client;
    int msg_id;
    switch ((esp_mqtt_event_id_t)event_id) {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
        mqtt_connected = true;
        msg_id = esp_mqtt_client_subscribe(client, "esp32/alert", 0);
        ESP_LOGI(TAG, "subscribed to topic , msg_id=%d", msg_id);
        break;
    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
        mqtt_connected = false;
        break;

    case MQTT_EVENT_SUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d, return code=0x%02x ", event->msg_id, (uint8_t)*event->data);
        break;
    case MQTT_EVENT_UNSUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_PUBLISHED:
        ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_DATA:
        ESP_LOGI(TAG, "MQTT_EVENT_DATA");
        printf("TOPIC=%.*s\r\n", event->topic_len, event->topic);
        printf("DATA=%.*s\r\n", event->data_len, event->data);

        if (event->topic_len == (int)strlen("esp32/alert") && strncmp(event->topic, "esp32/alert", event->topic_len) == 0) {
            gpio_set_level(2, 1);
            vTaskDelay(pdMS_TO_TICKS(500));
            gpio_set_level(2, 0);
        }

        break;
    case MQTT_EVENT_ERROR:
        ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
        if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) {
            ESP_LOGI(TAG, "Last error code reported from esp-tls: 0x%x", event->error_handle->esp_tls_last_esp_err);
            ESP_LOGI(TAG, "Last tls stack error number: 0x%x", event->error_handle->esp_tls_stack_err);
            ESP_LOGI(TAG, "Last captured errno : %d (%s)", event->error_handle->esp_transport_sock_errno,
                     strerror(event->error_handle->esp_transport_sock_errno));
        } else if (event->error_handle->error_type == MQTT_ERROR_TYPE_CONNECTION_REFUSED) {
            ESP_LOGI(TAG, "Connection refused error: 0x%x", event->error_handle->connect_return_code);
        } else {
            ESP_LOGW(TAG, "Unknown error type: 0x%x", event->error_handle->error_type);
        }
        break;
    default:
        ESP_LOGI(TAG, "Other event id:%d", event->event_id);
        break;
    }
}

static void mqtt_app_start(void)
{
    const esp_mqtt_client_config_t mqtt_cfg = {
        .broker = {
            .address.uri = CONFIG_EXAMPLE_MQTT_BROKER_URI,
#if CONFIG_EXAMPLE_BROKER_CERTIFICATE_OVERRIDDEN
            .verification.certificate = cert_override_pem,
#elif CONFIG_EXAMPLE_CERT_VALIDATE_MOSQUITTO_CA
            .verification.certificate = (const char *)mosquitto_org_crt_start,
#else
            .verification.crt_bundle_attach = esp_crt_bundle_attach, /* Use built-in certificate bundle */
#endif
        },
        .credentials = {
            .username = "USERANAME",
            .authentication.password = "PASSWORD",
        }
    };

    ESP_LOGI(TAG, "[APP] Free memory: %" PRIu32 " bytes", esp_get_free_heap_size());
    mqtt_client = esp_mqtt_client_init(&mqtt_cfg);

    esp_mqtt_client_register_event(mqtt_client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(mqtt_client);
}


void app_main(void)
{
    ESP_LOGI(TAG, "[APP] Startup..");
    ESP_LOGI(TAG, "[APP] Free memory: %" PRIu32 " bytes", esp_get_free_heap_size());
    ESP_LOGI(TAG, "[APP] IDF version: %s", esp_get_idf_version());

    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    ESP_ERROR_CHECK(example_connect());
    gpio_set_direction(2, GPIO_MODE_OUTPUT);
    mqtt_app_start();


}
