#include <string.h>
#include "mqtt_processing.h"

mqtt_client_t *mqtt_client;

void mqtt_connection_cb(mqtt_client_t *client, void *arg, mqtt_connection_status_t status)
{
    if (status == MQTT_CONNECT_ACCEPTED) {
        printf("Connected to MQTT broker\n");
        mqtt_subscribe(client, "topic/sub", 1, mqtt_sub_request_cb, NULL);
    } else {
        printf("Connection failed: %d\n", status);
    }
}

void mqtt_sub_request_cb(void *arg, err_t result)
{
    if (result == ERR_OK) {
        printf("Subscribed to topic\n");
    } else {
        printf("Failed to subscribe: %d\n", result);
    }
}

void mqtt_publish_cb(void *arg, err_t result)
{
    if (result == ERR_OK) {
        //printf("Message published\n");
    } else {
        printf("Failed to publish: %d\n", result);
    }
}

void mqtt_incoming_publish_cb(void *arg, const char *topic, u32_t tot_len)
{
    printf("Incoming publish: %s, length: %d\n", topic, tot_len);
}

void mqtt_incoming_data_cb(void *arg, const u8_t *data, u16_t len, u8_t flags)
{
    printf("Incoming data: %.*s\n", len, data);
}

void mqtt_init(void)
{
    ip_addr_t broker_ip;
    IP4_ADDR(&broker_ip, 46, 29, 239, 137);

    mqtt_client = mqtt_client_new();
    if (mqtt_client == NULL) {
        printf("Failed to create MQTT client\n");
        return;
    }

    struct mqtt_connect_client_info_t ci;
    memset(&ci, 0, sizeof(ci));
    ci.client_id = "lwip_client";
    ci.keep_alive = 60;
    ci.client_user = "igtrefilov";
    ci.client_pass = "estrella";

    mqtt_set_inpub_callback(mqtt_client, mqtt_incoming_publish_cb, mqtt_incoming_data_cb, NULL);

    err_t err = mqtt_client_connect(mqtt_client, &broker_ip, 1883, mqtt_connection_cb, NULL, &ci);
    if (err != ERR_OK) {
        printf("Failed to connect to MQTT broker: %d\n", err);
    }
}

void mqtt_publish_binary(const uint8_t *data, size_t len)
{
    err_t err = mqtt_publish(mqtt_client, "topic/pub", data, len, 1, 0, mqtt_publish_cb, NULL);
    if (err != ERR_OK) {
        //printf("Failed to publish binary message: %d\n", err);
    }
}
