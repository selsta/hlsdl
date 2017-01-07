#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>
#include <assert.h>
#include "msg.h"
#include "curl.h"
#include "hls.h"

struct MemoryStruct {
    char *memory;
    size_t size;
};

static size_t
WriteMemoryCallback(void *contents, size_t size, size_t nmemb, void *userp)
{
    size_t realsize = size * nmemb;
    struct MemoryStruct *mem = (struct MemoryStruct *)userp;

    mem->memory = realloc(mem->memory, mem->size + realsize + 1);
    if (mem->memory == NULL) {
        MSG_ERROR("not enough memory (realloc returned NULL)\n");
        return 0;
    }

    memcpy(&(mem->memory[mem->size]), contents, realsize);
    mem->size += realsize;
    mem->memory[mem->size] = 0;

    return realsize;
}


void * init_http_session(void)
{
    CURL *c;
    c = curl_easy_init();
    return c;
}

long get_data_from_url_with_session(void **session, char **url, char **out, size_t *size, int type, bool update_url)
{
    assert(session && *session);
    assert(url && *url);
    assert(size);

    CURL *c = (CURL *)(*session);
    CURLcode res;
    long http_code = 0;
    char *e_url = NULL;

    (*url)[strcspn(*url, "\r")] = '\0';

    struct MemoryStruct chunk;

    chunk.memory = malloc(1);
    chunk.size = 0;

    curl_easy_setopt(c, CURLOPT_URL, *url);
    curl_easy_setopt(c, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
    curl_easy_setopt(c, CURLOPT_WRITEDATA, (void *)&chunk);
    curl_easy_setopt(c, CURLOPT_USERAGENT, USER_AGENT);
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

void clean_http_session(void *session)
{
    curl_easy_cleanup((CURL *)session);
}

long get_data_from_url_ext(char **url, char **out, size_t *size, int type, bool update_url)
{
    CURL *c = (CURL *)init_http_session();
    long http_code = get_data_from_url_with_session(&c, url, out, size, type, update_url);
    clean_http_session(c);
    return http_code;
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
