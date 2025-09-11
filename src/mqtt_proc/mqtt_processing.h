#ifndef MQTT_PROCESSING_H
#define MQTT_PROCESSING_H

#include "mqtt.h"
#include "lwip/ip_addr.h"

void mqtt_connection_cb(mqtt_client_t *client, void *arg, mqtt_connection_status_t status);
void mqtt_sub_request_cb(void *arg, err_t result);
void mqtt_publish_cb(void *arg, err_t result);
void mqtt_incoming_publish_cb(void *arg, const char *topic, u32_t tot_len);
void mqtt_incoming_data_cb(void *arg, const u8_t *data, u16_t len, u8_t flags);
void mqtt_init(void);
void mqtt_publish_binary(const uint8_t *data, size_t len);
void mqtt_sub_request_cb(void *arg, err_t result);

#endif // MQTT_PROCESSING_H
