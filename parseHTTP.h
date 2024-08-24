#ifndef __PARSEHTTP__
#define MAX_PAR_COUNT 10
#define MAX_PAR_LEN 32
#define MAX_COMMAND_LEN 255
#define URL_NOT_MATCH 1
#define URL_MATCH_OK 0
#define CALLBACK_NO_MATCH 255
#define PARSE_API_NAME "api"
#define PARSE_API_NAME_LEN 3
enum HTTP_PARAM_TYPES {
    GET_PARAM,
    POST_PARAM,
    OPTIONS_PARAM,
};
enum MIME_TYPES {
    MIME_PLAIN_TEXT,
    MIME_JSON,
    MIME_STREAM,
    MIME_CUSTOM = 255
};
typedef int (*httpCallback)(char** argv,int argc,char** resp,int* resp_code);
typedef struct {
    const char *className;
    const char *subClassName;
    const char valueName[MAX_PAR_COUNT][MAX_PAR_LEN];
    int valueCount;
    httpCallback fn;
    enum HTTP_PARAM_TYPES paramType;
    enum MIME_TYPES mimeType;
    const char *customMimeType;
}httpCallback_t;
int url_parse(char* url,char* className,int* classNameLen,char* subClassName,int* subClassNameLen);
#endif