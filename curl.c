#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>
#include "msg.h"
#include "curl.h"

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
        /* out of memory! */
        MSG_ERROR("not enough memory (realloc returned NULL)\n");
        return 0;
    }
    
    memcpy(&(mem->memory[mem->size]), contents, realsize);
    mem->size += realsize;
    mem->memory[mem->size] = 0;
    
    return realsize;
}

size_t get_data_from_url(char *url, char **data, int type)
{
    CURL *c;
    CURLcode res;
    
    url[strcspn(url, "\r")] = '\0';
    
    struct MemoryStruct chunk;
    
    chunk.memory = malloc(1);
    chunk.size = 0;
    
    c = curl_easy_init();
    curl_easy_setopt(c, CURLOPT_URL, url);
    curl_easy_setopt(c, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
    curl_easy_setopt(c, CURLOPT_WRITEDATA, (void *)&chunk);
    curl_easy_setopt(c, CURLOPT_USERAGENT, USER_AGENT);
    
    res = curl_easy_perform(c);
    
    if (res != CURLE_OK) {
        MSG_ERROR("curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
    } else {
        if (type == STRING) {
            *data = strdup(chunk.memory);
        } else if (type == HEXSTR) {
            *data = (char*)malloc(33);
            int length = 0;
            for (int i = 0; i < 16; i++) {
                length += snprintf((*data)+length, 33 , "%02x", (unsigned char)chunk.memory[i]);
            }
            (*data)[32] = '\0';
        } else if (type == BINARY) {
            *data = (char*)malloc(chunk.size);
            *data = memcpy(*data, chunk.memory, chunk.size);
        }
    }
    
    curl_easy_cleanup(c);
    
    if (chunk.memory) {
        free(chunk.memory);
    }
    
    return chunk.size;
}