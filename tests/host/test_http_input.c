#include "shared/protocols/http/bvstk_http_input.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void)
{
    char out[32]; uint32_t n;
    assert(bvstk_http_query("/x?path=%2Fa%2Bb%20c", "path", out, sizeof(out)) == 1);
    assert(!strcmp(out, "/a+b c"));
    const char *bad[] = {"/x?path=%00a", "/x?path=%", "/x?path=%2", "/x?path=%GG", "/x?path=a&path=b", "/x?path=a&p%61th=b", "/x?path=%0A"};
    for (size_t i = 0; i < sizeof(bad)/sizeof(bad[0]); ++i) assert(bvstk_http_query(bad[i], "path", out, sizeof(out)) == -1);
    assert(bvstk_http_query("/x?path=abc", "path", out, 3) == -1);
    assert(bvstk_http_query("/x", "path", out, sizeof(out)) == 0);
    assert(bvstk_http_uint("4294967295",UINT32_MAX,&n) && n == UINT32_MAX);
    assert(!bvstk_http_uint("4294967296",UINT32_MAX,&n));
    assert(!bvstk_http_uint("-1",UINT32_MAX,&n));
    assert(!bvstk_http_uint("31x",31,&n));
    const char *paths[] = {"../x", "/../x", "/x/../a", "/./x", "/a/", "/a//b", "/a.", "/a ", "/a/b:", "/a\\b", "/a?b", "/a*"};
    for (size_t i = 0; i < sizeof(paths)/sizeof(paths[0]); ++i) assert(!bvstk_http_path(paths[i]));
    assert(bvstk_http_path("/")); assert(bvstk_http_path("/config/mdio/lan8720.json"));
    assert(bvstk_http_path("/a..b"));
    const char *const keys[] = {"name","rules","enabled"};
    assert(bvstk_http_json_object("{\"name\":\"x\",\"rules\":[{\"reg\":1,\"val\":2}],\"enabled\":true}",keys,3,7));
    const char *json[] = {"{}", "{\"name\":\"x\",}", "{\"name\":\"x\",\"name\":\"y\"}",
        "{\"name\":\"x\",\"unknown\":1}", "{\"name\":\"x\",\"rules\":[{\"reg\":1,\"reg\":2}]}",
        "{\"name\":\"x\",\"rules\":[1,]}", "{\"name\":\"x\",\"rules\":[01]}",
        "{\"name\":\"x\",\"rules\":[1.5]}", "{\"name\":\"x\",\"rules\":[-1]}",
        "{\"name\":\"x\",\"rules\":null}", "{\"name\":\"x\"}x", "{\"name\":\"\\u00\"}",
        "{\"name\":\"x\",\"rules\":[[[[[[[[[1]]]]]]]]]}", "[1]"};
    for (size_t i = 0; i < sizeof(json)/sizeof(json[0]); ++i) assert(!bvstk_http_json_object(json[i],keys,3,1));
    puts("HTTP input tests passed"); return 0;
}
