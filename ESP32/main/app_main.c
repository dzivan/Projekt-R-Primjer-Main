#include <inttypes.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "sdkconfig.h"

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_system.h>
#include <tsl2591.h>


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
        //Subscribing to the input command topic
        msg_id = esp_mqtt_client_subscribe(client, "esp32/command", 1);
        ESP_LOGI(TAG, "subscribed to topic , msg_id=%d", msg_id);
        break;
    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
        mqtt_connected = false;
        break;
    //Mostly cases used just for logging
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
    };

    ESP_LOGI(TAG, "[APP] Free memory: %" PRIu32 " bytes", esp_get_free_heap_size());
    mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
    /* The last argument may be used to pass data to the event handler, in this example mqtt_event_handler */
    esp_mqtt_client_register_event(mqtt_client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(mqtt_client);
}
//Light sensor function, only called once
void tsl2591_test(void *pvParameters)
{
    tsl2591_t dev = { 0 };

    ESP_ERROR_CHECK(tsl2591_init_desc(&dev, 0, CONFIG_EXAMPLE_I2C_MASTER_SDA, CONFIG_EXAMPLE_I2C_MASTER_SCL));
    ESP_ERROR_CHECK(tsl2591_init(&dev));

    // Turn TSL2591 on
    ESP_ERROR_CHECK(tsl2591_set_power_status(&dev, TSL2591_POWER_ON));
    // Turn ALS on
    ESP_ERROR_CHECK(tsl2591_set_als_status(&dev, TSL2591_ALS_ON));
    // Set gain
    ESP_ERROR_CHECK(tsl2591_set_gain(&dev, TSL2591_GAIN_MEDIUM));
    // Set integration time = 300ms
    ESP_ERROR_CHECK(tsl2591_set_integration_time(&dev, TSL2591_INTEGRATION_300MS));

    float lux;
    char msg[64];
    esp_err_t res;
    //Endless read loop for the light sensor
    while (1)
    {
        vTaskDelay(pdMS_TO_TICKS(500));

        if ((res = tsl2591_get_lux(&dev, &lux)) != ESP_OK) {
            printf("Could not read lux value: %d\n", res);
            continue;
    }
        else {
            printf("Lux: %f\n", lux);
            if (mqtt_connected) {
                //Publishing all light readings
                snprintf(msg, sizeof(msg), "{\"lux\":%.2f}", lux);
                esp_mqtt_client_publish(mqtt_client, "esp32/light", msg, 0, 0, 0);
            }
        
        }
        

    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "[APP] Startup..");
    ESP_LOGI(TAG, "[APP] Free memory: %" PRIu32 " bytes", esp_get_free_heap_size());
    ESP_LOGI(TAG, "[APP] IDF version: %s", esp_get_idf_version());

    esp_log_level_set("*", ESP_LOG_INFO);
    esp_log_level_set("esp-tls", ESP_LOG_VERBOSE);
    esp_log_level_set("mqtt_client", ESP_LOG_VERBOSE);
    esp_log_level_set("mqtt_example", ESP_LOG_VERBOSE);
    esp_log_level_set("transport_base", ESP_LOG_VERBOSE);
    esp_log_level_set("transport", ESP_LOG_VERBOSE);
    esp_log_level_set("outbox", ESP_LOG_VERBOSE);

    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    ESP_ERROR_CHECK(i2cdev_init());
    /* This helper function configures Wi-Fi or Ethernet, as selected in menuconfig.
     * Read "Establishing Wi-Fi or Ethernet Connection" section in
     * examples/protocols/README.md for more information about this function.
     */
    ESP_ERROR_CHECK(example_connect());

    mqtt_app_start();
    //Enable parallel readings of light intesity
    xTaskCreatePinnedToCore(tsl2591_test, "tsl2591_test", configMINIMAL_STACK_SIZE * 8, NULL, 5, NULL, APP_CPU_NUM);


}
