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

static size_t write_data(void *ptr, size_t size, size_t nmemb, void *stream)
{
    int written = (int)fwrite(ptr, size, nmemb, (FILE *)stream);
    return written;
}

int dl_file(char *url, char *name)
{
    CURL *c;
    FILE *fp;
    CURLcode res;
    int errorcode = 0;
    char outfilename[FILENAME_MAX];
    strcpy(outfilename, name);
    
    c = curl_easy_init();
    
    if (c) {
        url[strcspn(url, "\r")] = '\0';
        fp = fopen(outfilename,"wb");
        curl_easy_setopt(c, CURLOPT_URL, url);
        curl_easy_setopt(c, CURLOPT_WRITEFUNCTION, write_data);
        curl_easy_setopt(c, CURLOPT_WRITEDATA, fp);
        curl_easy_setopt(c, CURLOPT_USERAGENT, USER_AGENT);
        res = curl_easy_perform(c);
        
        if (res != CURLE_OK) {
            MSG_ERROR("curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
            errorcode = 1;
        }
        
        /* always cleanup */
        curl_easy_cleanup(c);
        fclose(fp);
    }
    return 0;
}

int get_source_from_url(char *url, char **source)
{
    CURL *c;
    CURLcode res;
    
    url[strcspn(url, "\r")] = '\0';
    
    int errorcode = 0;
    
    struct MemoryStruct chunk;
    
    chunk.memory = malloc(1);  /* will be grown as needed by the realloc above */
    chunk.size = 0;    /* no data at this point */
    
    c = curl_easy_init();
    curl_easy_setopt(c, CURLOPT_URL, url);
    curl_easy_setopt(c, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
    curl_easy_setopt(c, CURLOPT_WRITEDATA, (void *)&chunk);
    curl_easy_setopt(c, CURLOPT_USERAGENT, USER_AGENT);
    
    res = curl_easy_perform(c);
    
    /* check for errors */
    if (res != CURLE_OK) {
        MSG_ERROR("curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
        errorcode = 1;
    } else {
        *source = strdup(chunk.memory);
    }
    
    curl_easy_cleanup(c);
    
    if (chunk.memory) {
        free(chunk.memory);
    }
    
    return errorcode;
}

int get_hex_from_url(char *url, char hex[])
{
    CURL *c;
    CURLcode res;
    
    url[strcspn(url, "\r")] = '\0';
    
    int errorcode = 0;
    
    struct MemoryStruct chunk;
    
    chunk.memory = malloc(1);  /* will be grown as needed by the realloc above */
    chunk.size = 0;    /* no data at this point */
    
    c = curl_easy_init();
    curl_easy_setopt(c, CURLOPT_URL, url);
    curl_easy_setopt(c, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
    curl_easy_setopt(c, CURLOPT_WRITEDATA, (void *)&chunk);
    curl_easy_setopt(c, CURLOPT_USERAGENT, USER_AGENT);
    
    res = curl_easy_perform(c);
    
    /* check for errors */
    if (res != CURLE_OK) {
        MSG_ERROR("curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
        errorcode = 1;
    } else {
        int length = 0;
        for (int i = 0; i < 16; i++) {
            length += snprintf(hex+length, 33 , "%02x", (unsigned char)chunk.memory[i]);
        }
    }
    
    curl_easy_cleanup(c);
    
    if (chunk.memory) {
        free(chunk.memory);
    }
    
    return errorcode;
}