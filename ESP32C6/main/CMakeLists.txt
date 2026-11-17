idf_component_register(
    SRCS "app_main.c"
    PRIV_REQUIRES esp_driver_gpio esp-tls esp_wifi esp_event nvs_flash
    INCLUDE_DIRS "."
    EMBED_TXTFILES "mosquitto.org.crt"
)
