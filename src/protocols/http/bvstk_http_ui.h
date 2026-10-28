#ifndef BVSTK_HTTP_UI_H
#define BVSTK_HTTP_UI_H
#include "shared/protocols/http/bvstk_http.h"
#include "services/control/bvstk_control_api.h"
#include "services/fs/bvstk_fs_storage.h"
#include "shared/events/bvstk_journal.h"
#include "services/system/bvstk_system.h"
#include "shared/interfaces/bvstk_guard.h"
int bvstk_http_system(bvstk_system_t *,const bvstk_guard_t *,const http_request_t *,http_conn_t *);
#define HTTP_UI_JSON_CAP 65537U
typedef union { i2c_device_config_t i2c; mdio_phy_config_t mdio; } http_bus_config_t;
typedef struct {
    bvstk_i2c_master_service_t *(*i2c)(void);
    bvstk_mdio_service_t *(*mdio)(void);
    bvstk_status_t (*snapshot)(int, size_t, http_bus_config_t *);
    bvstk_status_t (*apply)(int, size_t, const http_bus_config_t *, const char *revision);
    int (*save)(int, size_t, const http_bus_config_t *);
    void (*sync)(size_t);
    const bvstk_fs_storage_t *(*storage)(void);
    void (*volumes)(int);
    void (*sd_info)(int);
    void *(*allocate)(size_t);
    void (*release)(void *);
    void (*observe)(unsigned,unsigned,int,int,uint64_t,bvstk_journal_batch_t *);
    bool fat_timestamps;
} http_ui_backend_t;
const http_ui_backend_t *http_ui_backend(void);
int http_server_write(int, const void *, size_t);
int http_ui_handle(const http_request_t *,http_conn_t *);
int http_legacy_uint(const char *,const char *,uint32_t *);
int http_legacy_bool(const char *,const char *,int *);
int http_legacy_string(const char *,const char *,char *,size_t);
int http_ui_legacy_i2c(const http_request_t *,http_conn_t *);
int http_ui_bus(const http_request_t *,http_conn_t *);
int http_ui_files(const http_request_t *,http_conn_t *);
int http_ui_observe(const http_request_t *,http_conn_t *);
int http_ui_send(int,const void *,size_t);
void http_ui_text(int,const char *);
void http_ui_printf(int,const char *,...);
void http_ui_string(int,const char *);
void http_ui_header(int,const char *,long long);
void http_ui_error(int,int,const char *);
void http_ui_status(int,bvstk_status_t);
int http_ui_body(const http_request_t *,http_conn_t *,char *,size_t);
int http_ui_param(const http_request_t *,http_conn_t *,const char *,char *,size_t);
#endif
