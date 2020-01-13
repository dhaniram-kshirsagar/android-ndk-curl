/*
 * Copyright (C) 2016 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */
#include <cstring>
#include <jni.h>
#include <cinttypes>
#include <android/log.h>
#include <gmath.h>
#include <gperf.h>
#include <curl/curl.h>
#include <string>

#define LOGI(...) \
  ((void)__android_log_print(ANDROID_LOG_INFO, "jni-curl-lib::", __VA_ARGS__))

struct data {
    char trace_ascii; /* 1 or 0 */
};

static
void dump(const char *text,
          FILE *stream, unsigned char *ptr, size_t size,
          char nohex)
{
    size_t i;
    size_t c;

    unsigned int width = 0x10;

    if(nohex)
        /* without the hex output, we can fit more on screen */
        width = 0x40;

    LOGI("%s, %10.10lu bytes (0x%8.8lx)\n" PRIu64,
            text, (unsigned long)size, (unsigned long)size);

    for(i = 0; i<size; i += width) {

        LOGI("%4.4lx: " PRIu64, (unsigned long)i);

        if(!nohex) {
            /* hex not disabled, show it */
            for(c = 0; c < width; c++)
                if(i + c < size)
                    LOGI("%02x " PRIu64, ptr[i + c]);
                else
                    LOGI("   " PRIu64);
        }

        for(c = 0; (c < width) && (i + c < size); c++) {
            /* check for 0D0A; if found, skip past and start a new line of output */
            if(nohex && (i + c + 1 < size) && ptr[i + c] == 0x0D &&
               ptr[i + c + 1] == 0x0A) {
                i += (c + 2 - width);
                break;
            }
            LOGI("%c" PRIu64,
                    (ptr[i + c] >= 0x20) && (ptr[i + c]<0x80)?ptr[i + c]:'.');
            /* check again for 0D0A, to avoid an extra \n if it's at width */
            if(nohex && (i + c + 2 < size) && ptr[i + c + 1] == 0x0D &&
               ptr[i + c + 2] == 0x0A) {
                i += (c + 3 - width);
                break;
            }
        }
        LOGI("\n" PRIu64); /* newline */
    }
}

static
int my_trace(CURL *handle, curl_infotype type,
             char *data, size_t size,
             void *userp)
{
    struct data *config = (struct data *)userp;
    const char *text;
    (void)handle; /* prevent compiler warning */

    switch(type) {
        case CURLINFO_TEXT:
            LOGI("== Info: %s" PRIu64, data);
            /* FALLTHROUGH */
        default: /* in case a new one is introduced to shock us */
            return 0;

        case CURLINFO_HEADER_OUT:
            text = "=> Send header";
            break;
        case CURLINFO_DATA_OUT:
            text = "=> Send data";
            break;
        case CURLINFO_SSL_DATA_OUT:
            text = "=> Send SSL data";
            break;
        case CURLINFO_HEADER_IN:
            text = "<= Recv header";
            break;
        case CURLINFO_DATA_IN:
            text = "<= Recv data";
            break;
        case CURLINFO_SSL_DATA_IN:
            text = "<= Recv SSL data";
            break;
    }

    dump(text, stderr, (unsigned char *)data, size, config->trace_ascii);
    return 0;
}

/* silly test data to POST */
static const char data[]="Lorem ipsum dolor sit amet, consectetur adipiscing "
                         "elit. Sed vel urna neque. Ut quis leo metus. Quisque eleifend, ex at "
                         "laoreet rhoncus, odio ipsum semper metus, at tempus ante urna in mauris. "
                         "Suspendisse ornare tempor venenatis. Ut dui neque, pellentesque a varius "
                         "eget, mattis vitae ligula. Fusce ut pharetra est. Ut ullamcorper mi ac "
                         "sollicitudin semper. Praesent sit amet tellus varius, posuere nulla non, "
                         "rhoncus ipsum.";

struct WriteThis {
    const char *readptr;
    size_t sizeleft;
};

static size_t read_callback(void *dest, size_t size, size_t nmemb, void *userp)
{
    struct WriteThis *wt = (struct WriteThis *)userp;
    size_t buffer_size = size*nmemb;

    if(wt->sizeleft) {
        /* copy as much as possible from the source to the destination */
        size_t copy_this_much = wt->sizeleft;
        if(copy_this_much > buffer_size)
            copy_this_much = buffer_size;
        memcpy(dest, wt->readptr, copy_this_much);

        wt->readptr += copy_this_much;
        wt->sizeleft -= copy_this_much;
        return copy_this_much; /* we copied this many bytes */
    }

    return 0; /* no more data left to deliver */
}

int postData(void)
{
    LOGI("inside postdata" PRIu64);

    CURL *curl;
    CURLcode res;

    struct data config;

    config.trace_ascii = 1; /* enable ascii tracing */

    char errbuf[CURL_ERROR_SIZE];

    struct WriteThis wt;

    wt.readptr = data;
    wt.sizeleft = strlen(data);

    /* In windows, this will init the winsock stuff */
    curl_global_init(CURL_GLOBAL_ALL);

    /* get a curl handle */
    curl = curl_easy_init();
    if(curl) {
        curl_easy_setopt(curl, CURLOPT_DEBUGFUNCTION, my_trace);
        curl_easy_setopt(curl, CURLOPT_DEBUGDATA, &config);
        /* example.com is redirected, so we tell libcurl to follow redirection */
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

        /* First set the URL that is about to receive our POST. */
        curl_easy_setopt(curl, CURLOPT_URL, "http://10.0.2.2:5000/data");

        /* provide a buffer to store errors in */
        curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, errbuf);

        /* set the error buffer as empty before performing a request */
        errbuf[0] = 0;

        static const char *postthis = "moo mooo moo moo";
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, postthis);

        /* if we don't provide POSTFIELDSIZE, libcurl will strlen() by
           itself */
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, (long)strlen(postthis));

//        /* Now specify we want to POST data */
//        curl_easy_setopt(curl, CURLOPT_HTTPPOST, 1L);
//
//        /* we want to use our own read function */
//        curl_easy_setopt(curl, CURLOPT_READFUNCTION, read_callback);
//
//        /* pointer to pass to our read function */
//        curl_easy_setopt(curl, CURLOPT_READDATA, &wt);

        /* get verbose debug output please */
        curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);

        /*
          If you use POST to a HTTP 1.1 server, you can send data without knowing
          the size before starting the POST if you use chunked encoding. You
          enable this by adding a header like "Transfer-Encoding: chunked" with
          CURLOPT_HTTPHEADER. With HTTP 1.0 or without chunked transfer, you must
          specify the size in the request.
        */
#ifdef USE_CHUNKED
        {
      struct curl_slist *chunk = NULL;

      chunk = curl_slist_append(chunk, "Transfer-Encoding: chunked");
      res = curl_easy_setopt(curl, CURLOPT_HTTPHEADER, chunk);
      /* use curl_slist_free_all() after the *perform() call to free this
         list again */
    }
#else
        /* Set the expected POST size. If you want to POST large amounts of data,
           consider CURLOPT_POSTFIELDSIZE_LARGE */
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, (long) wt.sizeleft);
#endif

#ifdef DISABLE_EXPECT
        /*
      Using POST with HTTP 1.1 implies the use of a "Expect: 100-continue"
      header.  You can disable this header with CURLOPT_HTTPHEADER as usual.
      NOTE: if you want chunked transfer too, you need to combine these two
      since you can only set one list of headers with CURLOPT_HTTPHEADER. */

    /* A less good option would be to enforce HTTP 1.0, but that might also
       have other implications. */
    {
      struct curl_slist *chunk = NULL;

      chunk = curl_slist_append(chunk, "Expect:");
      res = curl_easy_setopt(curl, CURLOPT_HTTPHEADER, chunk);
      /* use curl_slist_free_all() after the *perform() call to free this
         list again */
    }
#endif

        /* Perform the request, res will get the return code */
        res = curl_easy_perform(curl);
        /* Check for errors */
        if (res != CURLE_OK) {
            size_t len = strlen(errbuf);
            fprintf(stderr, "\nlibcurl: (%d) ", res);
            if (len)
                LOGI("Error in curl : %s%s" PRIu64, errbuf,
                        ((errbuf[len - 1] != '\n') ? "\n" : ""));
            else
                LOGI("curl_easy_perform() failed: %s"
                             PRIu64, curl_easy_strerror(res));
        }

        /* always cleanup */
        curl_easy_cleanup(curl);
    }
    curl_global_cleanup();
    return 0;
}
  /* This is a trivial JNI example where we use a native method
 * to return a new VM String. See the corresponding Java source
 * file located at:
 *
 *   app/src/main/java/com/example/jnicurl/MainActivity.java
 */
extern "C" JNIEXPORT jstring JNICALL
Java_com_example_jnicurl_MainActivity_stringFromJNI(JNIEnv *env, jobject thiz) {
    // Just for simplicity, we do this right away; correct way would do it in
    // another thread...
    auto ticks = GetTicks();

    for (auto exp = 0; exp < 32; ++exp) {
        volatile unsigned val = gpower(exp);
        (void) val;  // to silence compiler warning
    }
    ticks = GetTicks() - ticks;

    LOGI("calculation time: %" PRIu64, ticks);
    postData();

    // Post message to local server


    return env->NewStringUTF("Hello from JNI LIBS!");
}