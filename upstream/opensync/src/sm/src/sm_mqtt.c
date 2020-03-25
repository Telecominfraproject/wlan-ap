/*
Copyright (c) 2015, Plume Design Inc. All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
   1. Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
   2. Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
   3. Neither the name of the Plume Design Inc. nor the
      names of its contributors may be used to endorse or promote products
      derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL Plume Design Inc. BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include <limits.h>
#include <stdio.h>
#include <zlib.h>

#include "os_time.h"
#include "os_nif.h"
#include "mosqev.h"
#include "dppline.h"
#include "target.h"
#include "log.h"

#include "sm.h"

#define MODULE_ID LOG_MODULE_ID_MAIN

#define STATS_MQTT_PORT         8883
#define STATS_MQTT_QOS          0
#define STATS_MQTT_RECONNECT    60  /* Reconnect interval -- seconds */

/* Global MQTT instance */
static mosqev_t         sm_mqtt;
static bool             sm_mosquitto_init = false;
static bool             sm_mosqev_init = false;
static struct ev_timer  sm_mqtt_timer;
static int64_t          sm_mqtt_reconnect = 0;
static char             sm_mqtt_broker[HOST_NAME_MAX];
static char             sm_mqtt_topic[HOST_NAME_MAX];
static uint8_t          sm_mqtt_buf[STATS_MQTT_BUF_SZ];
static int              sm_mqtt_port = STATS_MQTT_PORT;
static int              sm_mqtt_qos = STATS_MQTT_QOS;
static uint8_t          sm_mqtt_compress = 0;

static mosqev_log_cbk_t sm_mqtt_log;
static void sm_mqtt_timer_handler(struct ev_loop *loop, ev_timer *timer, int revents);
bool sm_mqtt_publish(mosqev_t *mqtt, long mlen, void *mbuf);


/*
 * Initialize MQTT library
 */
bool sm_mqtt_init(void)
{
    /* Mosqev doesn't initialize libmosquitto */
    mosquitto_lib_init();

    sm_mosquitto_init = true;

    LOG(INFO,
        "Initializing MQTT library.\n");
    /*
     * Use the device serial number as client ID
     */
    char cID[64];
    if (true != target_id_get(cID, sizeof(cID)))
    {
        LOGE("acquiring device id number\n");
        goto error;
    }

    if (!mosqev_init(&sm_mqtt, cID, EV_DEFAULT, NULL))
    {
        LOGE("initializing MQTT library.\n");
        goto error;
    }

    /* Initialize logging */
    mosqev_log_cbk_set(&sm_mqtt, sm_mqtt_log);

    sm_mosqev_init = true;

    /* Start the MQTT reconnect/report timer */
    ev_timer_init(&sm_mqtt_timer, sm_mqtt_timer_handler,
            STATS_MQTT_INTERVAL, STATS_MQTT_INTERVAL);

    sm_mqtt_timer.data = &sm_mqtt;

    ev_timer_start(EV_DEFAULT, &sm_mqtt_timer);

    return true;

error:
    sm_mqtt_stop();

    return false;
}

/**
 * Set MQTT settings
 */
void sm_mqtt_set(const char *broker, const char *port, const char *topic, const char *qos, int compress)
{
    sm_mqtt_compress = compress;

    if (broker != NULL)
    {
        STRSCPY(sm_mqtt_broker, broker);
    }
    else
    {
        sm_mqtt_broker[0] = '\0';
    }

    if (port)
    {
        sm_mqtt_port = atoi(port);
    }
    else
    {
        sm_mqtt_port = STATS_MQTT_PORT;
    }

    if (qos)
    {
        sm_mqtt_qos = atoi(qos);
    }
    else
    {
        sm_mqtt_qos = STATS_MQTT_QOS;
    }

    if (topic != NULL)
    {
        STRSCPY(sm_mqtt_topic, topic);
    }
    else
    {
        sm_mqtt_topic[0] = '\0';
    }

    (void)qos; // Unused

    /* Initialize TLS stuff */
    if (!mosqev_tls_opts_set(&sm_mqtt, SSL_VERIFY_PEER, MOSQEV_TLS_VERSION, mosqev_ciphers))
    {
        LOGE("setting TLS options.\n");
        goto error;
    }

    if (!mosqev_tls_set(&sm_mqtt,
                        target_tls_cacert_filename(),
                        NULL,
                        target_tls_mycert_filename(),
                        target_tls_privkey_filename(),
                        NULL))
    {
        LOGE("setting TLS certificates.\n");
        goto error;
    }

    LOGN("MQTT broker \"%s\", topic \"%s\", CA cert \"%s\" compress: %d",
            sm_mqtt_broker, sm_mqtt_topic, ca_cert, sm_mqtt_compress);
    return;

error:
    sm_mqtt_stop();
    return;
}

bool sm_mqtt_config_valid(void)
{
    return
        (strlen(sm_mqtt_broker) > 0) &&
        (strlen(sm_mqtt_topic) > 0);
}

void sm_mqtt_stop(void)
{
    ev_timer_stop(EV_DEFAULT, &sm_mqtt_timer);

    if (sm_mosqev_init) mosqev_del(&sm_mqtt);
    if (sm_mosquitto_init) mosquitto_lib_cleanup();

    sm_mosqev_init = sm_mosquitto_init = false;

    LOG(NOTICE, "Closing MQTT connection.");
}

void sm_mqtt_timer_handler(struct ev_loop *loop, ev_timer *timer, int revents)
{
    (void)loop;
    (void)timer;
    (void)revents;

    uint32_t buf_len;

    mosqev_t *mqtt = timer->data;

    /*
     * Reconnect handler
     */
    if (sm_mqtt_config_valid())
    {
        if (!mosqev_is_connected(mqtt))
        {
            if (sm_mqtt_reconnect < ticks())
            {
                sm_mqtt_reconnect = ticks() + TICKS_S(STATS_MQTT_RECONNECT);

                LOG(DEBUG, "Connecting to %s ...\n", sm_mqtt_broker);
                if (!mosqev_connect(&sm_mqtt, sm_mqtt_broker, sm_mqtt_port))
                {
                    LOGE("Connecting.\n");
                    return;
                }
            }
            else
            {
                LOG(DEBUG, "Not connected, will reconnect in %"PRId64" ticks", sm_mqtt_reconnect - ticks());
            }
        }
    }
    else
    {
        /*
         * Config is invalid, but we're connected ... disconnect at once.
         */
        sm_mqtt_reconnect = 0;

        if (mosqev_is_connected(mqtt))
        {
            mosqev_disconnect(mqtt);
            return;
        }
    }

    /* Do not report any stats if we're not connected */
    if (!mosqev_is_connected(mqtt))
    {
        return;
    }

    /*
     * Publish statistics report to MQTT
     */
    LOG(DEBUG, "Total %d elements queued for transmission.\n", dpp_get_queue_elements());

    while (dpp_get_queue_elements() > 0)
    {
        if (!dpp_get_report(sm_mqtt_buf, sizeof(sm_mqtt_buf), &buf_len))
        {
            LOGE("DPP: Get report failed.\n");
            break;
        }

        if (buf_len <= 0) continue;

        if (!sm_mqtt_publish(mqtt, buf_len, sm_mqtt_buf))
        {
            LOGE("Publish report failed.\n");
            break;
        }
    }
}

bool sm_mqtt_publish(mosqev_t *mqtt, long mlen, void *mbuf)
{
    int ret;
    static unsigned long buflen = 0;
    static unsigned char *buf = NULL;
    long len;

    if (sm_mqtt_compress)
    {
        /*
         * cppcheck detects the code below as a memory leak. This is not the
         * case as buf is merely used as a dynamically growing buffer for
         * compresssed data.
         *
         * mosqev_publish() will ultimately copy the data to its own buffers.
         */
        len = compressBound(mlen);
        if (len > (long)buflen)
        {
            // allocate 1/4 more than needed
            buflen = len + len / 4;
            buf = realloc(buf, buflen);
            if (!buf)
            {
                LOGE("DPP: allocate compress buf (%d): out of mem.", buflen);
                return false;
            }
        }
        len = buflen;
        ret = compress(buf, (unsigned long*)&len, mbuf, mlen);
        if (ret != Z_OK)
        {
            LOGE("DPP: compression error %d", ret);
            return false;
        }
        LOGD("DPP: Publishing uncompressed: %d compressed: %d reduction: %d%%",
                    mlen, len, (int)(100 - 100 * len / mlen));
        mlen = len;
        mbuf = buf;
    }
    else
    {
        LOGD("DPP: Publishing %d bytes", mlen);
    }
    return mosqev_publish(mqtt, NULL, sm_mqtt_topic, mlen, mbuf, sm_mqtt_qos, false);
}

void sm_mqtt_log(mosqev_t *mqtt, void *data, int lvl, const char *str)
{
    (void)mqtt;
    (void)data;
    (void)lvl;

    LOGD("MQTT LOG: %s\n", str);
}
