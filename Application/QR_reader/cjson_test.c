#include <stdio.h>
#include <cjson/cJSON.h>

int main(void)
{
    const char *txt = "{\"v\":1,\"cmd\":\"alarm\",\"date\":\"2026-11-20\",\"time\":\"07:30:00\"}";
    cJSON *root = cJSON_Parse(txt);
    if (!root) {
        fprintf(stderr, "parse error near: %s\n", cJSON_GetErrorPtr());
        return 1;
    }
    cJSON *cmd = cJSON_GetObjectItemCaseSensitive(root, "cmd");
    cJSON *time = cJSON_GetObjectItemCaseSensitive(root, "time");
    if (cJSON_IsString(cmd) && cJSON_IsString(time))
        printf("cmd=%s time=%s\n", cmd->valuestring, time->valuestring);
    cJSON_Delete(root);
    return 0;
}