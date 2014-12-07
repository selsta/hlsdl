#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>

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
    if(mem->memory == NULL) {
        /* out of memory! */
        printf("not enough memory (realloc returned NULL)\n");
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
    CURL *curl_handle;
    FILE *fp;
    CURLcode res;
    int errorcode = 0;
    char outfilename[FILENAME_MAX];
    strcpy(outfilename, name);
    
    curl_handle = curl_easy_init();
    
    if (curl_handle) {
        fp = fopen(outfilename,"wb");
        curl_easy_setopt(curl_handle, CURLOPT_URL, url);
        curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, write_data);
        curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, fp);
        curl_easy_setopt(curl_handle, CURLOPT_USERAGENT, "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/28.0.1500.52 Safari/537.36");
        res = curl_easy_perform(curl_handle);
        
        
        if(res != CURLE_OK) {
            fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
            errorcode = 1;
        }
        /* always cleanup */
        curl_easy_cleanup(curl_handle);
        fclose(fp);
    }
    return 0;
}


int get_source_from_url(const char *url, char **source)
{
    CURL *curl_handle;
    CURLcode res;
    
    int errorcode = 0;
    
    struct MemoryStruct chunk;
    
    chunk.memory = malloc(1);  /* will be grown as needed by the realloc above */
    chunk.size = 0;    /* no data at this point */
    
    /* init the curl session */
    curl_handle = curl_easy_init();
    
    /* specify URL to get */
    curl_easy_setopt(curl_handle, CURLOPT_URL, url);
    
    /* send all data to this function  */
    curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
    
    /* we pass our 'chunk' struct to the callback function */
    curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, (void *)&chunk);
    
    /* some servers don't like requests that are made without a user-agent
     field, so we provide one */
    curl_easy_setopt(curl_handle, CURLOPT_USERAGENT, "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/28.0.1500.52 Safari/537.36");
    
    /* get it! */
    res = curl_easy_perform(curl_handle);
    
    /* check for errors */
    if(res != CURLE_OK) {
        fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
        errorcode = 1;
    } else {
        *source = strdup(chunk.memory);
    }
    
    /* cleanup curl stuff */
    curl_easy_cleanup(curl_handle);
    
    if(chunk.memory)
        free(chunk.memory);

    return errorcode;
}