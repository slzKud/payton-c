/*
 * lws-minimal-http-server-form-post
 *
 * Written in 2010-2019 by Andy Green <andy@warmcat.com>
 *
 * This file is made available under the Creative Commons CC0 1.0
 * Universal Public Domain Dedication.
 *
 * This demonstrates a minimal http server that performs POST with a couple
 * of parameters.  It dumps the parameters to the console log and redirects
 * to another page.
 */

#include <libwebsockets.h>
#include <string.h>
#include "parseHTTP.h"

/*
 * Unlike ws, http is a stateless protocol.  This pss only exists for the
 * duration of a single http transaction.  With http/1.1 keep-alive and http/2,
 * that is unrelated to (shorter than) the lifetime of the network connection.
 */
struct pss {
	char path[128];
	char className[MAX_PAR_LEN];
	int classNameLen;
	char subClassName[MAX_PAR_LEN];
	int subClassNameLen;
	int enableParse;
    int content_lines;
	enum HTTP_PARAM_TYPES paramType;
	char *resp;
	int resp_code;
	int ret;
	char *mimeType[MAX_PAR_LEN];
	struct lws_spa *spa;
};
const char mimeList[][MAX_PAR_LEN]={"text/plain","application/json","application/octet-stream"};
int testPrg1(char** argv,int argc,char** resp,int* resp_code);
int configJson(char** argv,int argc,char** resp,int* resp_code);
int gen_code_200(char** argv,int argc,char** resp,int* resp_code);
int gen_code_500(char** argv,int argc,char** resp,int* resp_code);
int doCallback(struct lws *wsi,void *pss,const char* className,const char* subClassName,enum HTTP_PARAM_TYPES paramType,char **resp,int* resp_code);
int getGEToPOST(const char* className,const char* subClassName);
int getCallbackID(const char* className,const char* subClassName);
int getMimeType(const char* className,const char* subClassName,char* mimeType);
httpCallback_t callBacks[]={
    {"Hello","World",{"name"},1,testPrg1,GET_PARAM,MIME_PLAIN_TEXT,NULL},
	{"Hello","Post",{"name"},1,testPrg1,POST_PARAM,MIME_PLAIN_TEXT,NULL},
	{"Hello","HTTP_CODE_200",{},0,gen_code_200,GET_PARAM,MIME_PLAIN_TEXT,NULL},
	{"Hello","HTTP_CODE_500",{},0,gen_code_500,GET_PARAM,MIME_PLAIN_TEXT,NULL},
};
static int
callback_http(struct lws *wsi, enum lws_callback_reasons reason, void *user,
	      void *in, size_t len)
{
	struct pss *pss = (struct pss *)user;
	uint8_t buf[LWS_PRE + LWS_RECOMMENDED_MIN_HEADER_SPACE], *start = &buf[LWS_PRE],
		*p = start, *end = &buf[sizeof(buf) - 1];
	int n;

	switch (reason) {
	case LWS_CALLBACK_HTTP:

		lwsl_user("LWS_CALLBACK_HTTP");
    	lws_snprintf(pss->path, sizeof(pss->path), "%s",
                        (const char *)in);

        lws_get_peer_simple(wsi, (char *)buf, sizeof(buf));
        lwsl_notice("%s: HTTP: connection %s, path %s\n", __func__,
                        (const char *)buf, pss->path);
		if(url_parse(pss->path,pss->className,&pss->classNameLen,pss->subClassName,&pss->subClassNameLen)==URL_MATCH_OK){
			lwsl_user("Parse MATCH:%s/%s",pss->className,pss->subClassName);
			pss->enableParse=1;
			pss->paramType=getGEToPOST(pss->className,pss->subClassName);
		}else{
			pss->enableParse=2;
			pss->paramType=GET_PARAM;
			pss->resp_code=500;
			sprintf(pss->mimeType,"%s","application/json");
		}
		if(pss->enableParse==1){
			pss->resp_code=200;
			sprintf(pss->mimeType,"%s",mimeList[1]);
			pss->ret=doCallback(wsi,pss,pss->className,pss->subClassName,pss->paramType,&pss->resp,&pss->resp_code);
			lwsl_user("callback:%d",pss->ret);
			if(pss->ret==CALLBACK_NO_MATCH){
				pss->resp_code=404;
			}else{
				getMimeType(pss->className,pss->subClassName,pss->mimeType);
			}
		}
		if(pss->paramType==GET_PARAM){
			lwsl_user("pss->resp_code:%d,pss->mime:%s",pss->resp_code,pss->mimeType);
			if (lws_add_http_common_headers(wsi, pss->resp_code,
                        	pss->mimeType,
                        	LWS_ILLEGAL_HTTP_CONTENT_LEN,
                        	&p, end))
                	return 1;
            if (lws_finalize_write_http_header(wsi, start, &p, end))
                return 1;

			/* write the body separately */
            lws_callback_on_writable(wsi);
			return 0;
		}else{
			return 0;
		}
		break;
	case LWS_CALLBACK_HTTP_WRITEABLE:
			lwsl_user("LWS_CALLBACK_HTTP_WRITEABLE");
            if (!pss)
                break;
			
			if(pss->enableParse==1){
				if(pss->ret==CALLBACK_NO_MATCH){
					lwsl_user("callback not match");
					p += lws_snprintf((char *)p, lws_ptr_diff_size_t(end, p), "{\"code\":-999,\"info\":\"api dont match.\"}");
				}else{
					if(pss->resp!=NULL)
						p += lws_snprintf((char *)p, lws_ptr_diff_size_t(end, p), "%s",pss->resp);
					else
						p += lws_snprintf((char *)p, lws_ptr_diff_size_t(end, p), "{\"code\":0,\"info\":\"this api dont have resp.\"}");
				}
				//free(resp);
			}else{
				p += lws_snprintf((char *)p, lws_ptr_diff_size_t(end, p), "{\"code\":-999,\"info\":\"URL parse error.\"}");
				//p += lws_snprintf((char *)p, lws_ptr_diff_size_t(end, p), "<p>URL parse error.</p>");
			}
            if (lws_write(wsi, (uint8_t *)start, lws_ptr_diff_size_t(p, start), (enum lws_write_protocol)n) !=
                    lws_ptr_diff(p, start))
                    return 1;
            if (lws_http_transaction_completed(wsi))
                    return -1;
            return 0;
	case LWS_CALLBACK_HTTP_BODY:

		/* create the POST argument parser if not already existing */
		char **param;
		size_t j,valueCount=0;
		int i;
		i=getCallbackID(pss->className,pss->subClassName);
		valueCount=callBacks[i].valueCount>MAX_PAR_COUNT?MAX_PAR_COUNT:callBacks[i].valueCount;
		param=(char **) malloc(valueCount * sizeof(char *));
        for (j = 0; j < valueCount; j++) {
			param[j]=(char *) malloc(MAX_PAR_LEN * sizeof(char));
			strncpy(param[j],callBacks[i].valueName[j],MAX_PAR_LEN);
		}
		if (!pss->spa) {
			pss->spa = lws_spa_create(wsi, (const char * const *)param,
					LWS_ARRAY_SIZE(param), 1024,
					NULL, NULL); /* no file upload */
			if (!pss->spa)
				return -1;
		}

		/* let it parse the POST data */

		if (lws_spa_process(pss->spa, in, (int)len))
			return -1;
		break;

	case LWS_CALLBACK_CLOSED_CLIENT_HTTP:
		if (pss->spa && lws_spa_destroy(pss->spa))
			return -1;
		break;

	case LWS_CALLBACK_HTTP_BODY_COMPLETION:

		/* inform the spa no more payload data coming */

		lwsl_user("LWS_CALLBACK_HTTP_BODY_COMPLETION\n");
		lws_spa_finalize(pss->spa);
		lwsl_user("pss->resp_code:%d,pss->mime:%s",pss->resp_code,pss->mimeType);
		if (lws_add_http_common_headers(wsi, pss->resp_code,
                        pss->mimeType,
                        LWS_ILLEGAL_HTTP_CONTENT_LEN, /* no content len */
                        &p, end))
            return 1;
        if (lws_finalize_write_http_header(wsi, start, &p, end))
            return 1;
		lws_callback_on_writable(wsi);
		return 0;

	case LWS_CALLBACK_HTTP_DROP_PROTOCOL:
		/* called when our wsi user_space is going to be destroyed */
		if (pss->spa) {
			lws_spa_destroy(pss->spa);
			pss->spa = NULL;
		}
		break;

	default:
		break;
	}

	return lws_callback_http_dummy(wsi, reason, user, in, len);
}
int getGEToPOST(const char* className,const char* subClassName){
	size_t i;
	for (i = 0; i < sizeof(callBacks) / sizeof(callBacks[0]); i++) {
		if (strcmp(callBacks[i].className, className)==0 && strcmp(callBacks[i].subClassName, subClassName)==0) {
			return callBacks[i].paramType;
		}
	}
	return GET_PARAM;
}
int getMimeType(const char* className,const char* subClassName,char* mimeType){
	size_t i;
	for (i = 0; i < sizeof(callBacks) / sizeof(callBacks[0]); i++) {
		if (strcmp(callBacks[i].className, className)==0 && strcmp(callBacks[i].subClassName, subClassName)==0) {
			if(callBacks[i].mimeType==MIME_CUSTOM){
				sprintf(mimeType,"%s",callBacks[i].customMimeType);
				return 0;
			}else{
				sprintf(mimeType,"%s",mimeList[callBacks[i].mimeType]);
				return 0;
			}
		}
	}
	sprintf(*mimeType,"%s","text/plain");
	return -1;
}
int getCallbackID(const char* className,const char* subClassName){
	size_t i;
	for (i = 0; i < sizeof(callBacks) / sizeof(callBacks[0]); i++) {
		if (strcmp(callBacks[i].className, className)==0 && strcmp(callBacks[i].subClassName, subClassName)==0) {
			return i;
		}
	}
	return -1;
}
int doCallback(struct lws *wsi,void *pss,const char* className,const char* subClassName,enum HTTP_PARAM_TYPES paramType,char **resp,int *resp_code){
    size_t i,j,valueCount=0;
    char **valueArray=(char **) malloc(MAX_PAR_COUNT * sizeof(char *));
    char buf[255];
    int ret=CALLBACK_NO_MATCH;
    lwsl_user("className:%s/subClassName:%s\n",className,subClassName);
    for (i = 0; i < sizeof(callBacks) / sizeof(callBacks[0]); i++) {
        if (strcmp(callBacks[i].className, className)==0 && strcmp(callBacks[i].subClassName, subClassName)==0) {
            lwsl_user("Find : %s/%s\n",callBacks[i].className,callBacks[i].subClassName);
            if(callBacks[i].valueCount==0){
                free(valueArray);
                return callBacks[i].fn(NULL,0,resp,resp_code);
            }
            valueCount=callBacks[i].valueCount>MAX_PAR_COUNT?MAX_PAR_COUNT:callBacks[i].valueCount;
            lwsl_user("Value : %d,%d\n",sizeof(callBacks[i].valueName),sizeof(callBacks[i].valueName[0]));
            for (j = 0; j < valueCount; j++) {
                valueArray[j] = (char *) malloc(MAX_PAR_LEN * sizeof(char));
				memset(valueArray[j],0,MAX_PAR_LEN);
				if(paramType==GET_PARAM){
					int rv=lws_get_urlarg_by_name_safe(wsi, callBacks[i].valueName[j],
                                        (char *)buf, sizeof(buf));
					if(rv<0)
						continue;
				}else{
					struct pss *pss1=(struct pss *)pss;
					if(!lws_spa_get_string(pss1->spa, j))
						continue;
					sprintf(buf,"%s",lws_spa_get_string(pss1->spa, j));
				}
				strncpy(valueArray[j],buf,MAX_PAR_LEN);
            }
            ret=callBacks[i].fn(valueArray,valueCount,resp,resp_code);
        }
    }
    for (i = 0; i < valueCount; i++) 
    {
        free(valueArray[i]);
    }
    free(valueArray);
    return ret;
}
int testPrg1(char** argv,int argc,char** resp,int* resp_code){
	char *buf;
    buf=(char *)malloc(100 * sizeof(char));
    sprintf(buf,"From testprg1,Hello,World !name=%s",argv[0]);
    *resp=buf;
    return 0;
}
int gen_code_200(char** argv,int argc,char** resp,int* resp_code){
	char *buf;
    buf=(char *)malloc(100 * sizeof(char));
    sprintf(buf,"200 OK from rkkvmd");
    *resp=buf;
	*resp_code=200;
    return 0;
}
int gen_code_500(char** argv,int argc,char** resp,int* resp_code){
	char *buf;
    buf=(char *)malloc(100 * sizeof(char));
    sprintf(buf,"500 Interval Error from rkkvmd");
    *resp=buf;
	*resp_code=500;
    return 0;
}