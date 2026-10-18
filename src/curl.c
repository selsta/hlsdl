#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <limits.h>
#include <errno.h>
#include <pthread.h>
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
    char *cookie_file;
    void *cookie_file_mutex;
    long speed_limit;
    long speed_time;
};

struct MemoryStruct {
    char *memory;
    size_t size;
    size_t reserved;
    CURL *c;
};

void * set_timeout_session(void *ptr_session, const long speed_limit, const long speed_time)
{
    struct http_session *session = ptr_session;
    assert(session);
    session->speed_limit = speed_limit;
    session->speed_time = speed_time;

    return session;
}

/* first allocation when the server sends no Content-Length (chunked live
 * segments, compressed playlists) */
#define RECV_BUF_MIN (64 * 1024)
/* connecting gets this long; curl's own default is 300 s, which with the
 * segment retries left a dead CDN hanging the download for a long time */
#define CONNECT_TIMEOUT_SEC 15L

static size_t
WriteMemoryCallback(void *contents, size_t size, size_t nmemb, void *userp)
{
    size_t realsize = size * nmemb;
    struct MemoryStruct *mem = (struct MemoryStruct *)userp;

    /* mem->reserved = bytes allocated, the NUL terminator included */
    if (mem->reserved == 0)
    {
        CURLcode res;
        double filesize = 0.0;

        res = curl_easy_getinfo(mem->c, CURLINFO_CONTENT_LENGTH_DOWNLOAD, &filesize);
        /* more than a segment can hold (INT_MAX, see hls.c) is not trusted */
        if ((CURLE_OK == res) && (filesize > 0.0) && (filesize < (double)INT_MAX))
        {
            size_t want = (size_t)filesize + 1;
            char *tmp = realloc(mem->memory, want);
            if (tmp == NULL) {
                MSG_ERROR("not enough memory (realloc returned NULL)\n");
                return 0;
            }
            mem->memory = tmp;
            mem->reserved = want;
        }
    }

    if ((mem->size + realsize + 1) > mem->reserved)
    {
        /* Grow geometrically: without a Content-Length (or with the smaller
         * compressed one) every chunk curl hands over would otherwise mean a
         * realloc and, often, a copy of everything received so far. */
        size_t need = mem->size + realsize + 1;
        size_t want = mem->reserved < RECV_BUF_MIN ? RECV_BUF_MIN : mem->reserved * 2;
        if (want < need) {
            want = need;
        }
        char *tmp = realloc(mem->memory, want);
        if (tmp == NULL) {
            MSG_ERROR("not enough memory (realloc returned NULL)\n");
            return 0;
        }
        mem->memory = tmp;
        mem->reserved = want;
    }

    memcpy(&(mem->memory[mem->size]), contents, realsize);
    mem->size += realsize;
    mem->memory[mem->size] = 0;

    return realsize;
}

/* NULL when no memory or no curl handle could be had */
void * init_http_session(void)
{
    struct http_session *session = calloc(1, sizeof(struct http_session));
    if (!session) {
        return NULL;
    }
    session->handle = curl_easy_init();
    if (!session->handle) {
        free(session);
        return NULL;
    }
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

void * set_cookie_file_session(void *ptr_session, const char *cookie_file, void *cookie_file_mutex)
{
    struct http_session *session = ptr_session;
    assert(session);

    if (cookie_file) {
        if (session->cookie_file) {
            free(session->cookie_file);
        }
        session->cookie_file = strdup(cookie_file);
        session->cookie_file_mutex = cookie_file_mutex;
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

size_t get_data_from_localfile(char* filename, char** out, int64_t range_offset, int64_t range_size)
{
    int readsize = -1;
    FILE* fp;

    fp = fopen(filename, "rb");
    if (fp) {
        if (range_size < 0) {
            fseek(fp, 0, SEEK_END);
            readsize = ftell(fp);
            rewind(fp);
        }
        else {
            if (fseek(fp, range_offset, SEEK_SET))
            {
                MSG_ERROR("%s\n", strerror(errno));
                fclose(fp);
                return -1;
            }
            readsize = range_size;
        }

        if (readsize < 0) {
            MSG_ERROR("cannot determine the size of %s\n", filename);
            fclose(fp);
            return -1;
        }

        *out = (char*)malloc((size_t)readsize + 1);
        if (*out == NULL) {
            MSG_ERROR("out of memory\n");
            fclose(fp);
            return -1;
        }
        if (fread(*out, 1, (size_t)readsize, fp) != (size_t)readsize) {
            MSG_ERROR("fread returned less bytes than required\n");
            free(*out);
            *out = NULL;
            fclose(fp);
            return -1;
        }
        (*out)[readsize] = 0;
        fclose(fp);
    }
    else {
        MSG_ERROR("%s\n", strerror(errno));
        return -1;
    }
    return readsize;
}

long get_data_from_url_with_session(void **ptr_session, char *url, char **out, size_t *size, int type, char **new_url, int64_t range_offset, int64_t range_size)
{
    if (!strstr(url, "://")) {
        int fsize = get_data_from_localfile(url, out, range_offset, range_size);
        *size = fsize;
        if (new_url)
        {
            free(*new_url);
            *new_url = strdup(url);
        }
        return fsize > 0 ? 200 : 404;
    }

    assert(ptr_session && *ptr_session);
    struct http_session *session = *ptr_session;
    struct curl_slist *headers = session->headers;

    assert(session->handle);
    assert(url);
    assert(size);

    CURL *c = (CURL *)(session->handle);
    CURLcode res;
    long http_code = 0;
    char *e_url = NULL;

    url[strcspn(url, "\r")] = '\0';

    struct MemoryStruct chunk;

    chunk.memory = malloc(1);
    chunk.memory[0] = '\0';
    chunk.size = 0;
    chunk.reserved = 0;
    chunk.c = c;

    /* two full int64 values: "-9223372036854775808-9223372036854775807" */
    char range_buff[42];
    char* range = NULL;
    if (range_size > -1) {
        snprintf(range_buff, sizeof(range_buff), "%"PRId64"-%"PRId64, range_offset, range_offset + range_size - 1);
        range = range_buff;
    }

    curl_easy_setopt(c, CURLOPT_URL, url);
    curl_easy_setopt(c, CURLOPT_RANGE, range);
    curl_easy_setopt(c, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
    curl_easy_setopt(c, CURLOPT_WRITEDATA, (void *)&chunk);
    //curl_easy_setopt(c, CURLOPT_VERBOSE, 1L);

    if (session->speed_limit) {
        curl_easy_setopt(c, CURLOPT_LOW_SPEED_LIMIT, session->speed_limit);
    }

    if (session->speed_time) {
        curl_easy_setopt(c, CURLOPT_LOW_SPEED_TIME, session->speed_time);
    }

    curl_easy_setopt(c, CURLOPT_CONNECTTIMEOUT, CONNECT_TIMEOUT_SEC);
    curl_easy_setopt(c, CURLOPT_SSL_VERIFYPEER, 0L);
    /* curl_easy_setopt(c, CURLOPT_FRESH_CONNECT, 1);*/
    /* Compression only for playlists: segments and keys are binary (nothing
     * to gain), and a Range request answered with a compressed body would
     * apply the range to the compressed bytes. NULL switches it off again on
     * a reused handle. */
    curl_easy_setopt(c, CURLOPT_ACCEPT_ENCODING, type == STRING ? "" : NULL);

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

    if (session->cookie_file) {
        curl_easy_setopt(c, CURLOPT_COOKIEJAR, session->cookie_file);
        curl_easy_setopt(c, CURLOPT_COOKIEFILE, session->cookie_file);

        if (session->cookie_file_mutex) {
            pthread_mutex_lock(session->cookie_file_mutex);
            curl_easy_setopt(c, CURLOPT_COOKIELIST, "RELOAD");
            pthread_mutex_unlock(session->cookie_file_mutex);
        }
    }

    curl_easy_setopt(c, CURLOPT_FOLLOWLOCATION, 1L);

    res = curl_easy_perform(c);

    curl_easy_getinfo(c, CURLINFO_RESPONSE_CODE, &http_code);
    if (new_url && CURLE_OK == curl_easy_getinfo(c, CURLINFO_EFFECTIVE_URL, &e_url))
    {
        free(*new_url);
        *new_url = strdup(e_url);
    }

    if (res != CURLE_OK) {
        MSG_ERROR("curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
        /* a transfer that broke off after a 206 is just as incomplete */
        if (http_code == 200 || http_code == 206) {
            http_code = -(long)res;
        }
    } else {
        if (type == STRING || type == BINARY) {
            /* Hand the (always NUL-terminated) receive buffer over instead of
             * copying it - a segment would otherwise sit in memory twice. An
             * image in front of the segment data (e.g. the 1x1 PNG described
             * at https://laurentmeyer.medium.com/deep-dive-in-the-illegal-streaming-world-cd11fae63497)
             * is cut off in hls.c (strip_disguise_prefix), which checks for the
             * TS data behind it - not here. */
            *out = chunk.memory;
            chunk.memory = NULL;
        } else if (type == BINKEY) {
            /* a short key response must not be read as 16 bytes; the caller
             * rejects the size */
            *out = NULL;
            if (chunk.size == KEYLEN) {
                *out = malloc(KEYLEN);
                if (*out) {
                    memcpy(*out, chunk.memory, KEYLEN);
                }
            }
        }
    }

    *size = chunk.size;

    if (chunk.memory) {
        free(chunk.memory);
    }

    if (session->cookie_file && session->cookie_file_mutex) {
        pthread_mutex_lock(session->cookie_file_mutex);
        curl_easy_setopt(c, CURLOPT_COOKIELIST, "FLUSH");
        pthread_mutex_unlock(session->cookie_file_mutex);
    }

    return http_code;
}

void clean_http_session(void *ptr_session)
{
    struct http_session *session = ptr_session;
    curl_easy_cleanup(session->handle);

    if (session->user_agent) {
        free(session->user_agent);
    }

    if (session->proxy_uri) {
        free(session->proxy_uri);
    }

    if (session->cookie_file) {
        free(session->cookie_file);
    }

    /* free the custom headers if set */
    if (session->headers) {
        curl_slist_free_all(session->headers);
    }

    free(session);
}

size_t get_data_from_url(char *url, char **str, uint8_t **bin, int type, char **new_url)
{
    CURL *c = (CURL *)init_http_session();
    size_t size;
    char *out = NULL;
    get_data_from_url_with_session(&c, url, &out, &size, type, new_url, -1, -1);

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
