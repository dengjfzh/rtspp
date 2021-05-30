#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <getopt.h>
#include <unistd.h>
#include <curl/curl.h>

struct CONFIG {
} g_conf;

int parse_cmdline(int argc, char *argv[]);
int test(const char *url);


int main(int argc, char* argv[])
{
    int ret = parse_cmdline(argc, argv);
    if ( ret <= 0 ) {
        return (0==ret) ? 0 : errno;
    }
    test("rtsp://wowzaec2demo.streamlock.net/vod/mp4:BigBuckBunny_115k.mov");
    printf("ok.\n");
    return 0;
}


int parse_cmdline(int argc, char *argv[])
{
    // usage
    const char *basename = "rtspp";
    if ( argc > 0 ) {
        basename = strrchr(argv[0], '/');
        basename = basename ? (basename+1) : argv[0];
    }
    char usage[1024];
    snprintf(usage, sizeof(usage),
             "%s: A simple RTSP load tool.\n"
             "usage: %s [options]\n"
             "options:\n"
             "    -h,--help               display this help and exit\n"
             "    -v,--version            output version information and exit\n"
             "",
             basename, basename);
    memset(&g_conf, 0, sizeof(g_conf));
    // parse options
    static struct option opts[] = {
        {"help"   , 0, 0, 'h'},
        {"version", 0, 0, 'v'},
        {NULL, 0, 0, 0},
    };
    int c;
    while ( (c = getopt_long_only(argc, argv, ":hv", opts, NULL)) != -1 ) {
        switch ( c ) {
            case 'h':
                printf("%s\n", usage);
                return 0;
            case 'v':
                printf("GIT_VERSION: %s\n", GIT_VERSION);
                printf("GIT_DATE: %s\n", GIT_DATE);
                return 0;
            default:
                fprintf(stderr, "Unknown option!\n");
                errno = EINVAL;
                return -1;
        }
    }
    if ( optind < argc ) {
        fprintf(stderr, "Unknown option: %s\n", argv[optind]);
        errno = EINVAL;
        return -1;
    }
    return 1;
}



/* error handling macros */
#define my_curl_easy_setopt(A, B, C)                             \
    cc = curl_easy_setopt((A), (B), (C));                        \
    if ( CURLE_OK != cc )                                        \
        fprintf(stderr, "Error: curl_easy_setopt(%s, %s, %s) failed: %d\n", \
                #A, #B, #C, cc);

#define my_curl_easy_perform(A)                                     \
    cc = curl_easy_perform(A);                                      \
    if ( CURLE_OK != cc )                                           \
        fprintf(stderr, "Error: curl_easy_perform(%s) failed: %d\n", #A, cc);


static size_t header_callback(char *buffer, size_t size, size_t nitems, void *userdata)
{
    char line[1024];
    size_t len = (nitems >= sizeof(line)) ? (sizeof(line)-1) : nitems;
    for ( ; len > 0 && ('\n' == buffer[len-1] || '\r' == buffer[len-1]); len-- );
    memcpy(line, buffer, len);
    line[len] = '\0';
    fprintf(stderr, "header[%04lu]: %s\n", len, line);
    /* now this callback can access the my_info struct */
    return nitems * size;
}

static size_t write_callback(void *data, size_t size, size_t nmemb, void *userp)
{
    fprintf(stderr, "body[%lu*%lu]\n", size, nmemb);
    return nmemb * size;
}

/* send RTSP OPTIONS request */
static void rtsp_options(CURL *curl, const char *uri)
{
  CURLcode cc = CURLE_OK;
  printf("\nRTSP: OPTIONS %s\n", uri);
  my_curl_easy_setopt(curl, CURLOPT_RTSP_STREAM_URI, uri);
  my_curl_easy_setopt(curl, CURLOPT_RTSP_REQUEST, (long)CURL_RTSPREQ_OPTIONS);
  my_curl_easy_perform(curl);
}

struct BUFINFO {
    void *buf;
    size_t size;
    size_t len;
};

static size_t write_sdp_callback(void *data, size_t size, size_t nmemb, void *userp)
{
    struct BUFINFO *bi = (struct BUFINFO *)userp;
    size_t len = size * nmemb;
    if ( (len + bi->len) >= bi->size ) {
        fprintf(stderr, "write_sdp_callback: buffer full, size=%lu, length=%lu, new_len=%lu\n", bi->size, bi->len, len);
        return len;
    }
    memcpy((char*)bi->buf + bi->len, data, len);
    bi->len += len;
    fprintf(stderr, "body[%lu*%lu]\n", size, nmemb);
    return len;
}

/* send RTSP DESCRIBE request and write sdp response to a file */
static int rtsp_describe(CURL *curl, const char *uri, char *sdp_buf, size_t sdp_buf_size)
{
    CURLcode cc = CURLE_OK;
    printf("\nRTSP: DESCRIBE %s\n", uri);
    struct BUFINFO bi = {sdp_buf, sdp_buf_size, 0};
    my_curl_easy_setopt(curl, CURLOPT_WRITEDATA, &bi);
    my_curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_sdp_callback);
    my_curl_easy_setopt(curl, CURLOPT_RTSP_REQUEST, (long)CURL_RTSPREQ_DESCRIBE);
    my_curl_easy_perform(curl);
    if ( CURLE_OK != cc ) {
        return -1;
    }
    my_curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    return bi.len;
}

/* send RTSP SETUP request */
static void rtsp_setup(CURL *curl, const char *uri, const char *transport)
{
  CURLcode cc = CURLE_OK;
  printf("\nRTSP: SETUP %s\n", uri);
  printf("      TRANSPORT %s\n", transport);
  my_curl_easy_setopt(curl, CURLOPT_RTSP_STREAM_URI, uri);
  my_curl_easy_setopt(curl, CURLOPT_RTSP_TRANSPORT, transport);
  my_curl_easy_setopt(curl, CURLOPT_RTSP_REQUEST, (long)CURL_RTSPREQ_SETUP);
  my_curl_easy_perform(curl);
}


/* send RTSP PLAY request */
static void rtsp_play(CURL *curl, const char *uri, const char *range)
{
  CURLcode cc = CURLE_OK;
  printf("\nRTSP: PLAY %s\n", uri);
  my_curl_easy_setopt(curl, CURLOPT_RTSP_STREAM_URI, uri);
  my_curl_easy_setopt(curl, CURLOPT_RANGE, range);
  my_curl_easy_setopt(curl, CURLOPT_RTSP_REQUEST, (long)CURL_RTSPREQ_PLAY);
  my_curl_easy_perform(curl);

  /* switch off using range again */
  my_curl_easy_setopt(curl, CURLOPT_RANGE, NULL);
}


/* send RTSP TEARDOWN request */
static void rtsp_teardown(CURL *curl, const char *uri)
{
  CURLcode cc = CURLE_OK;
  printf("\nRTSP: TEARDOWN %s\n", uri);
  my_curl_easy_setopt(curl, CURLOPT_RTSP_REQUEST, (long)CURL_RTSPREQ_TEARDOWN);
  my_curl_easy_perform(curl);
}

/* send RTSP TEARDOWN request */
static void rtsp_receive(CURL *curl)
{
  CURLcode cc = CURLE_OK;
  printf("\nRTSP: receive\n");
  my_curl_easy_setopt(curl, CURLOPT_RTSP_REQUEST, (long)CURL_RTSPREQ_RECEIVE);
  my_curl_easy_perform(curl);
}


/* convert url into an sdp filename */
static void get_sdp_filename(const char *url, char *sdp_filename,
                             size_t namelen)
{
  const char *s = strrchr(url, '/');
  strcpy(sdp_filename, "video.sdp");
  if(s != NULL) {
    s++;
    if(s[0] != '\0') {
      snprintf(sdp_filename, namelen, "%s.sdp", s);
    }
  }
}


/* scan sdp file for media control attribute */
static void get_media_control_attribute(const char *sdp_filename,
                                        char *control)
{
  int max_len = 256;
  char *s = (char*)malloc(max_len);
  FILE *sdp_fp = fopen(sdp_filename, "rb");
  control[0] = '\0';
  if(sdp_fp != NULL) {
    while(fgets(s, max_len - 2, sdp_fp) != NULL) {
      sscanf(s, " a = control: %s", control);
    }
    fclose(sdp_fp);
  }
  free(s);
}

int test(const char *url)
{
    CURLcode cc;
    cc = curl_global_init(CURL_GLOBAL_ALL);
    if ( CURLE_OK != cc ) {
        fprintf(stderr, "Error: call curl_global_init failed! cc=%d\n", cc);
        return -1;
    }
    CURL *curl;
    curl = curl_easy_init();
    if ( NULL == curl ) {
        fprintf(stderr, "Error: call curl_easy_init failed!\n");
        return -1;
    }
    my_curl_easy_setopt(curl, CURLOPT_VERBOSE, 0L);
    my_curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 1L);
    my_curl_easy_setopt(curl, CURLOPT_HEADERDATA, stdout);
    my_curl_easy_setopt(curl, CURLOPT_URL, url);
    my_curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, header_callback);
    my_curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);

    rtsp_options(curl, url);

    char sdp_buf[1024*16];
    int sdp_len = rtsp_describe(curl, url, sdp_buf, sizeof(sdp_buf));
    if ( sdp_len > 0 ) {
        if ( sizeof(sdp_buf) == sdp_len )
            sdp_len--;
        sdp_buf[sdp_len] = '\0';
        fprintf(stderr, "SDP: %s", sdp_buf);
    }

    char tmp[1024];
    snprintf(tmp, sizeof(tmp), "%s/%s", url, "trackID=1");
    rtsp_setup(curl, tmp, "RTP/AVP/TCP;unicast;interleaved=0-1");
    snprintf(tmp, sizeof(tmp), "%s/%s", url, "trackID=2");
    rtsp_setup(curl, tmp, "RTP/AVP/TCP;unicast;interleaved=2-3");


    rtsp_play(curl, url, "0.000-");

    time_t time_start = time(NULL);
    while ( (time(NULL) - time_start) <= 15 ) {
        rtsp_receive(curl);
    }

    rtsp_teardown(curl, url);

    curl_easy_cleanup(curl);

    printf("test.\n");
    return 0;
}
