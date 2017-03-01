#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>
#include <assert.h>
#include "msg.h"
#include "curl.h"
#include "hls.h"

struct http_session {
    void *handle;
    void *headers;
    char *user_agent;
    char *proxy_uri;
};

struct MemoryStruct {
    char *memory;
    size_t size;
    size_t reserved;
    CURL *c;
};

static size_t
WriteMemoryCallback(void *contents, size_t size, size_t nmemb, void *userp)
{
    size_t realsize = size * nmemb;
    struct MemoryStruct *mem = (struct MemoryStruct *)userp;
    
    if (mem->reserved == 0)
    {
        CURLcode res;
        double filesize = 0.0;

        res = curl_easy_getinfo(mem->c, CURLINFO_CONTENT_LENGTH_DOWNLOAD, &filesize);
        if((CURLE_OK == res) && (filesize>0.0))
        {
            mem->memory = realloc(mem->memory, (int)filesize + 2);
            if (mem->memory == NULL) {
                MSG_ERROR("not enough memory (realloc returned NULL)\n");
                return 0;
            }
            mem->reserved = (int)filesize + 1;
        }
    }
    
    if ((mem->size + realsize + 1) > mem->reserved)
    {
        mem->memory = realloc(mem->memory, mem->size + realsize + 1);
        mem->reserved = mem->size + realsize + 1;
        if (mem->memory == NULL) {
            MSG_ERROR("not enough memory (realloc returned NULL)\n");
            return 0;
        }
    }

    memcpy(&(mem->memory[mem->size]), contents, realsize);
    mem->size += realsize;
    mem->memory[mem->size] = 0;

    return realsize;
}

void * init_http_session(void)
{
    struct http_session *session = malloc(sizeof(struct http_session));
    CURL *c;
    memset(session, 0x00, sizeof(struct http_session));
    c = curl_easy_init();
    session->handle = c;
    return session;
}

void * set_user_agent_http_session(void *ptr_session, const char *user_agent)
{
    struct http_session *session = ptr_session;
    assert(session);
    
    if (user_agent) {
        if (session->user_agent) {
            free(session->user_agent);
        }
        session->user_agent = malloc(strlen(user_agent)+1);
        strcpy(session->user_agent, user_agent);
    }
    
    return session;
}

void * set_proxy_uri_http_session(void *ptr_session, const char *proxy_uri)
{
    struct http_session *session = ptr_session;
    assert(session);
    
    if (proxy_uri) {
        if (session->proxy_uri) {
            free(session->proxy_uri);
        }
        session->proxy_uri = strdup(proxy_uri);
    }
    
    return session;
}

void add_custom_header_http_session(void *ptr_session, const char *header)
{
    struct http_session *session = ptr_session;
    assert(session);
    if (header) {
        struct curl_slist *headers = session->headers;
        headers = curl_slist_append(headers, header);
        session->headers = headers;
    }
}

void set_fresh_connect_http_session(void *ptr_session, long val)
{
    struct http_session *session = ptr_session;
    CURL *c = (CURL *)(session->handle);
    curl_easy_setopt(c, CURLOPT_FRESH_CONNECT, val);    
}

long get_data_from_url_with_session(void **ptr_session, char **url, char **out, size_t *size, int type, bool update_url)
{
    assert(ptr_session && *ptr_session);
    struct http_session *session = *ptr_session;
    struct curl_slist *headers = session->headers;
    
    assert(session->handle);
    assert(url && *url);
    assert(size);
    
    CURL *c = (CURL *)(session->handle);
    CURLcode res;
    long http_code = 0;
    char *e_url = NULL;

    (*url)[strcspn(*url, "\r")] = '\0';

    struct MemoryStruct chunk;

    chunk.memory = malloc(1);
    chunk.memory[0] = '\0';
    chunk.size = 0;
    chunk.reserved = 0;
    chunk.c = c; 

    curl_easy_setopt(c, CURLOPT_URL, *url);
    curl_easy_setopt(c, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
    curl_easy_setopt(c, CURLOPT_WRITEDATA, (void *)&chunk);
    curl_easy_setopt(c, CURLOPT_LOW_SPEED_LIMIT, 2L);
    curl_easy_setopt(c, CURLOPT_LOW_SPEED_TIME, 3L); 
    curl_easy_setopt(c, CURLOPT_SSL_VERIFYPEER, 0L);
    
    /* enable all supported built-in compressions */
    curl_easy_setopt(c, CURLOPT_ACCEPT_ENCODING, "");
    
    if (session->user_agent) {
        curl_easy_setopt(c, CURLOPT_USERAGENT, session->user_agent);
    } else {
        curl_easy_setopt(c, CURLOPT_USERAGENT, USER_AGENT);
    }
    if (headers) {
        curl_easy_setopt(c, CURLOPT_HTTPHEADER, headers);
    }
    
    if (session->proxy_uri) {
        curl_easy_setopt(c, CURLOPT_PROXY, session->proxy_uri);
    }
    
    curl_easy_setopt(c, CURLOPT_FOLLOWLOCATION, 1L);

    res = curl_easy_perform(c);
    
    curl_easy_getinfo(c, CURLINFO_RESPONSE_CODE, &http_code);
    if (update_url && CURLE_OK == curl_easy_getinfo(c, CURLINFO_EFFECTIVE_URL, &e_url))
    {
        if (0 != strcmp(*url, e_url))
        {
            free(*url);
            *url = strdup(e_url);
        }
    }

    if (res != CURLE_OK) {
        MSG_ERROR("curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
        if (http_code == 200) {
            http_code = -res;
        }
    } else {
        if (type == STRING) {
            *out = strdup(chunk.memory);
        } else if (type == BINKEY) {
            *out = malloc(KEYLEN);
            *out = memcpy(*out, chunk.memory, KEYLEN);
        } else if (type == BINARY) {
            *out = malloc(chunk.size);
            *out = memcpy(*out, chunk.memory, chunk.size);
        }
    }
    
    *size = chunk.size;

    if (chunk.memory) {
        free(chunk.memory);
    }

    return http_code;
}

void clean_http_session(void *ptr_session)
{
    struct http_session *session = ptr_session;
    curl_easy_cleanup(session->handle);
    
    /* free user agent if set */ 
    if (session->user_agent) {
        free(session->user_agent);
    }
    
    /* free user agent if set */ 
    if (session->proxy_uri) {
        free(session->proxy_uri);
    }
    
    /* free the custom headers if set */ 
    if (session->headers) {
        curl_slist_free_all(session->headers);
    }
}

size_t get_data_from_url(char **url, char **str, uint8_t **bin, int type, bool update_url)
{
    CURL *c = (CURL *)init_http_session();
    size_t size;
    char *out = NULL;
    get_data_from_url_with_session(&c, url, &out, &size, type, update_url);
    
    switch (type){
    case STRING:
        *str = out;
        break;
    case BINARY:
    case BINKEY:
        *bin = (uint8_t *)out;
        break;
    default:
        break;
    }

    clean_http_session(c);

    return size;
}
