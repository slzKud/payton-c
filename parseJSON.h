#define PARSE_MSG_JSON_OK 0
#define PARSE_MSG_JSON_INVALID 1
#define PARSE_JSON_INVALID_TYPE 2
#define PARSE_JSON_INVALID_PAYLOAD 3
#ifdef MODS_LWS_LOG
    #include <libwebsockets.h>
    #define printLog lwsl_user
    #define printErr lwsl_err
#else
    #define printLog printf
    #define printErr printf
#endif
int parseMSGJSON(const char* msg,char** resp);