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
	char filename[128];		/* the filename of the uploaded file */
	unsigned long long file_length; /* the amount of bytes uploaded */
	int fd;				/* fd on file being saved */
	struct lws_spa *spa;
};
const char mimeList[][MAX_PAR_LEN]={"text/plain","application/json","application/octet-stream"};
int testPrg1(char** argv,int argc,char** resp,int* resp_code);
int configJson(char** argv,int argc,char** resp,int* resp_code);
int gen_code_200(char** argv,int argc,char** resp,int* resp_code);
int gen_code_500(char** argv,int argc,char** resp,int* resp_code);
int uploadFileSuccess(char** argv,int argc,char** resp,int* resp_code);
int doCallback(struct lws *wsi,void *pss,const char* className,const char* subClassName,enum HTTP_PARAM_TYPES paramType,char **resp,int* resp_code);
int getCallbackID(const char* className,const char* subClassName);
int getMimeType(const char* className,const char* subClassName,char* mimeType);
httpCallback_t callBacks[]={
    {"Hello","World",{"name"},1,testPrg1,GET_PARAM,MIME_PLAIN_TEXT,NULL},
	{"Hello","Post",{"name"},1,testPrg1,POST_PARAM,MIME_PLAIN_TEXT,NULL},
	{"Upload","UploadFile",{"!UploadFilePath!","!UploadFileLength!"},2,uploadFileSuccess,POST_PARAM,MIME_PLAIN_TEXT,NULL},
	{"Hello","HTTP_CODE_200",{},0,gen_code_200,GET_PARAM,MIME_PLAIN_TEXT,NULL},
	{"Hello","HTTP_CODE_500",{},0,gen_code_500,GET_PARAM,MIME_PLAIN_TEXT,NULL},
};
int genTempFile(char *filePath){
	char *tmpFile=NULL;
	int fd;
	tmpFile=malloc(sizeof(char)*64);
	sprintf(tmpFile,"/tmp/tmp_XXXXXX");
	if ((fd = mkstemp(tmpFile)) < 0)
		return NULL;
	sprintf(filePath,"%s",tmpFile);
	free(tmpFile);
	return fd;
}
static int
file_upload_cb(void *data, const char *name, const char *filename,
	       char *buf, int len, enum lws_spa_fileupload_states state)
{
	struct pss *pss = (struct pss *)data;

	switch (state) {
	case LWS_UFS_OPEN:
		/* take a copy of the provided filename */
		//lws_strncpy(pss->filename, filename, sizeof(pss->filename) - 1);
		/* remove any scary things like .. */
		//lws_filename_purify_inplace(pss->filename);
		/* open a file of that name for write in the cwd */
		//pss->fd = lws_open(pss->filename, O_CREAT | O_TRUNC | O_RDWR, 0600);
		pss->fd=genTempFile(pss->filename);
		if (pss->fd == -1) {
			lwsl_notice("Failed to open output file %s\n",
				    pss->filename);
			return 1;
		}
		break;
	case LWS_UFS_FINAL_CONTENT:
	case LWS_UFS_CONTENT:
		if (len) {
			int n;

			pss->file_length += (unsigned int)len;

			n = (int)write(pss->fd, buf, (unsigned int)len);
			if (n < len) {
				lwsl_notice("Problem writing file %d\n", errno);
			}
		}
		if (state == LWS_UFS_CONTENT)
			/* wasn't the last part of the file */
			break;

		/* the file upload is completed */

		lwsl_user("%s: upload done, written %lld to %s\n", __func__,
			  pss->file_length, pss->filename);

		close(pss->fd);
		pss->fd = -1;
		break;
	case LWS_UFS_CLOSE:
		break;
	}

	return 0;
}

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
		if(lws_hdr_total_length(wsi, WSI_TOKEN_GET_URI)>0)
			pss->paramType=GET_PARAM;
		if(lws_hdr_total_length(wsi, WSI_TOKEN_POST_URI)>0)
			pss->paramType=POST_PARAM;
		if(lws_hdr_total_length(wsi, WSI_TOKEN_OPTIONS_URI)>0)
			pss->paramType=OPTIONS_PARAM;
		enum lws_token_indexes i1 = WSI_TOKEN_OPTIONS_URI;
		int header_len = lws_hdr_total_length(wsi, i1);
		if(header_len>0){
			char *test_header_buf;
			test_header_buf=malloc(sizeof(char)*(header_len + 4));
			lws_hdr_copy(wsi, test_header_buf, header_len+4, i1);
			lwsl_user("Get WSI_TOKEN_OPTIONS_URI->%s",test_header_buf);
			pss->enableParse==999;
			
			if (lws_add_http_common_headers(wsi, 204,
                        	"text/plain",
                        	LWS_ILLEGAL_HTTP_CONTENT_LEN,
                        	&p, end))
                	return 1;
			//
			lws_add_http_header_by_name(wsi,"Access-Control-Allow-Origin:","*",1,&p,end);
			lws_add_http_header_by_name(wsi,"Access-Control-Allow-Headers:","Content-Type, Access-Control-Allow-Origin, Access-Control-Allow-Headers, X-Requested-By, Access-Control-Allow-Methods,OAMAuth",125,&p,end);
			lws_add_http_header_by_name(wsi,"Access-Control-Allow-Methods:","POST, GET, PUT, DELETE",22,&p,end);
            if (lws_finalize_write_http_header(wsi, start, &p, end))
                return 1;
			/* write the body separately */
            lws_callback_on_writable(wsi);
			return 0;
		}
        lws_get_peer_simple(wsi, (char *)buf, sizeof(buf));
        lwsl_notice("%s: HTTP: connection %s, path %s\n", __func__,
                        (const char *)buf, pss->path);
		if(url_parse(pss->path,pss->className,&pss->classNameLen,pss->subClassName,&pss->subClassNameLen)==URL_MATCH_OK){
			lwsl_user("Parse MATCH:%s/%s",pss->className,pss->subClassName);
			pss->enableParse=1;
		}else{
			pss->enableParse=2;
			pss->paramType=GET_PARAM;
			pss->resp_code=500;
			sprintf(pss->mimeType,"%s","application/json");
		}
		if(pss->enableParse==1 && pss->paramType==GET_PARAM){
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
			lws_add_http_header_by_name(wsi,"Access-Control-Allow-Origin:","*",1,&p,end);
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
			if(strcmp(pss->className,"Upload")==0 && strcmp(pss->subClassName,"UploadFile")==0){
				pss->spa = lws_spa_create(wsi, (const char * const *)param,
						valueCount, 1024,
						file_upload_cb, pss);
			}else{
				pss->spa = lws_spa_create(wsi, (const char * const *)param,
						valueCount, 1024,
						NULL, NULL);
			}
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
		if(pss->enableParse==1 && pss->paramType==POST_PARAM){
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
		lwsl_user("pss->resp_code:%d,pss->mime:%s",pss->resp_code,pss->mimeType);
		if (lws_add_http_common_headers(wsi, pss->resp_code,
                        pss->mimeType,
                        LWS_ILLEGAL_HTTP_CONTENT_LEN, /* no content len */
                        &p, end))
            return 1;
		lws_add_http_header_by_name(wsi,"Access-Control-Allow-Origin:","*",1,&p,end);
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
	struct pss *pss1=(struct pss *)pss;
    int ret=CALLBACK_NO_MATCH;
    lwsl_user("className:%s/subClassName:%s\n",className,subClassName);
    for (i = 0; i < sizeof(callBacks) / sizeof(callBacks[0]); i++) {
        if (strcmp(callBacks[i].className, className)==0 && strcmp(callBacks[i].subClassName, subClassName)==0 && callBacks[i].paramType==paramType) {
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
				if(strcmp(callBacks[i].valueName[j],"!UploadFilePath!")==0){
					lwsl_user("filename:%s\n",pss1->filename);
					if(strlen(pss1->filename)>0 && pss1->file_length>0){
						sprintf(valueArray[j],"%s",pss1->filename);
						continue;
					}
				}
				if(strcmp(callBacks[i].valueName[j],"!UploadFileLength!")==0){
					lwsl_user("length:%lld\n",pss1->file_length);
					if(strlen(pss1->filename)>0 && pss1->file_length>0){
						sprintf(valueArray[j],"%lld",pss1->file_length);
						continue;
					}
				}
				if(paramType==GET_PARAM){
					int rv=lws_get_urlarg_by_name_safe(wsi, callBacks[i].valueName[j],
                                        (char *)buf, sizeof(buf));
					if(rv<0)
						continue;
				}else{
					if(!lws_spa_get_string(pss1->spa, j))
						continue;
					sprintf(buf,"%s",lws_spa_get_string(pss1->spa, j));
				}
				strncpy(valueArray[j],buf,MAX_PAR_LEN);
            }
            ret=callBacks[i].fn(valueArray,valueCount,resp,resp_code);
			break;
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
int uploadFileSuccess(char** argv,int argc,char** resp,int* resp_code){
	char *buf;
    buf=(char *)malloc(100 * sizeof(char));
    sprintf(buf,"Upload File Success!Filename:%s,Filelength:%s",argv[0],argv[1]);
    *resp=buf;
	*resp_code=200;
    return 0;
}