#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "parseHTTP.h"
int url_parse(char* url,char* className,int* classNameLen,char* subClassName,int* subClassNameLen){
    //printf("url:%s\n",url);
    const char* splitChar = "/";
    char* ptr=NULL;
    int argc=0;
    char**pps8Output = (char **) malloc(MAX_PAR_COUNT * sizeof(char *));
    int ret=URL_MATCH_OK;
    ptr = strtok(url, splitChar);
	for(; ptr != NULL; )
	{
        if(argc>MAX_PAR_COUNT){
            break;
        }
        pps8Output[argc] = (char *) malloc(MAX_PAR_LEN * sizeof(char));
        strncpy(pps8Output[argc],ptr,MAX_PAR_LEN);
		//printf("%s\n", pps8Output[argc]);
        argc=argc+1;
		ptr = strtok(NULL, splitChar);
	}
    //command_excute(pps8Output,argc);
    if(argc<3){
        ret=URL_NOT_MATCH;
    }else{
        if(strcmp(PARSE_API_NAME,pps8Output[0])==0){
            strncpy(className,pps8Output[1],MAX_PAR_LEN);
            *classNameLen=strlen(pps8Output[1]);
            strncpy(subClassName,pps8Output[2],MAX_PAR_LEN);
            *subClassNameLen=strlen(pps8Output[2]);
        }else{
            ret=URL_NOT_MATCH;
        }
    }
    for (int i = 0; i < argc; i++) 
    {
        free(pps8Output[i]);
    }
    free(pps8Output);
    //printf("%d",argc);
    return ret;
}
