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

#include <stdbool.h>
#include <time.h>

#include "log.h"
#include "json_mqtt.h"
#include "os_types.h"

char *version = "1.0.0";

/**
 * @brief checks that a session has all the info to report through mqtt
 *
 * @param session the fsm session storing the header information
 * @return true if the mqtt topics and location/node ids have been set,
 *         false otherwise
 */
static bool
jcheck_header_info(struct fsm_session *session)
{
    if (session->topic == NULL) return false;
    if (session->location_id == NULL) return false;
    if (session->node_id == NULL) return false;

    return true;
}


/**
 * @brief gets the current time in the cloud requested format
 *
 * @param time_str input string which will store the timestamp
 * @param input string size
 *
 * Formats the current time in the cloud accepted format:
 * yyyy-mm-ddTHH:MM:SS.mmmZ.
 */
static void
json_mqtt_curtime(char *time_str, size_t size)
{
    struct timeval tv;
    struct tm *tm ;
    char tstr[50];

    gettimeofday(&tv, NULL);
    tm = gmtime(&tv.tv_sec);
    strftime(tstr, sizeof(tstr), "%FT%T", tm);
    snprintf(time_str, size, "%s.%03dZ", tstr, (int)(tv.tv_usec / 1000));
}


/**
 * @brief encodes the header section of a message
 *
 * @param session fsm session storing the header information
 * @param json_report json object getting filled
 *
 * Fills up the header section of a json formatted mqtt report
 */
void
jencode_header(struct fsm_session *session, json_t *json_report)
{
    char time_str[128] = { 0 };
    char *str;

    /* Encode mqtt headers section */
    str = session->location_id;
    json_object_set_new(json_report, "locationId", json_string(str));
    str = session->node_id;
    json_object_set_new(json_report, "nodeId", json_string(str));

    /* Encode version */
    json_object_set_new(json_report, "version", json_string(version));

    /* Encode report time */
    json_mqtt_curtime(time_str, sizeof(time_str));
    json_object_set_new(json_report, "reportedAt", json_string(time_str));
}


/**
 * @brief encodes a user agent report in json format
 *
 * @param session fsm session storing the header information
 * @param to_report http user agent information to report
 * @return a string containing the json encoded information.
 *
 * The caller needs to free the string pointer through a json_free() call.
 */
char *
jencode_user_agent(struct fsm_session *session,
                   struct http_parse_report *to_report)
{
    char str_mac[128] = { 0 };
    json_t *body_envelope;
    json_t *json_report;
    char *json_msg;
    json_t *body;
    bool ready;
    char *str;

    ready = jcheck_header_info(session);
    if (ready == false) return NULL;

    json_report  = json_object();
    body_envelope = json_array();
    body = json_object();

    /* Encode header */
    jencode_header(session, json_report);

    /* Encode body */
    snprintf(str_mac, sizeof(str_mac),
             PRI(os_macaddr_t), FMT(os_macaddr_t, to_report->src_mac));
    str = str_mac;
    json_object_set_new(body, "deviceMac", json_string(str));
    str = to_report->user_agent;
    json_object_set_new(body, "userAgent", json_string(str));

    /* Encode body envelope */
    json_array_append_new(body_envelope, body);
    json_object_set_new(json_report, "httpRequests", body_envelope);

    /* Convert json object in a compact string */
    json_msg = json_dumps(json_report, JSON_COMPACT);
    json_decref(json_report);

    return json_msg;
}


static char *actions[FSM_NUM_ACTIONS] =
{
    "none",
    "blocked",
    "allowed",
    "observed"
};

static char *cache_lookup_failure = "cacheLookupFailed";
static char *remote_lookup_failure = "remoteLookupFailed";


/**
 * @brief returns a string matching the action to report
 *
 * @param to_report information to report
 * @return the action string
 */
static inline char *
get_action_str(struct fqdn_pending_req *to_report)
{
    struct fsm_url_request *url_info;
    struct fsm_url_reply *cat_reply;

    if (to_report->categorized != FSM_FQDN_CAT_FAILED)
    {
        return actions[to_report->action];
    }

    url_info = to_report->req_info;
    cat_reply = url_info->reply;

     /* cache lookup error */
    if (cat_reply->lookup_status) return cache_lookup_failure;

    return remote_lookup_failure; /* remote lookup error */
}


static json_t *
jencode_wb_report(struct fsm_url_reply *reply)
{
    struct fsm_wp_info *info;
    json_t *categorization;
    json_t *categories;
    size_t nelems;
    size_t i;

    categorization = json_object();
    categories = json_array();
    info = &reply->wb;

    /* Encode categories */
    nelems = reply->nelems;
    for (i = 0; i < nelems; i++)
    {
        json_t *category;
        int category_id;

        category = json_object();
        category_id = reply->categories[i];
        /* Encode category and confidence */
        json_object_set_new(category, "categoryId",
                            json_integer(category_id));;

        /* Append category, confidence data to the categories array */
        json_array_append_new(categories, category);
    }

    /* Encode categorization */
    json_object_set_new(categorization, "riskLevel",
                        json_integer(info->risk_level));
    json_object_set_new(categorization, "categories",
                        categories);


    return categorization;
}


static json_t *
jencode_bc_report(struct fsm_url_reply *reply)
{
    struct fsm_bc_info *info;
    json_t *categorization;
    json_t *categories;
    size_t nelems;
    size_t i;

    categorization = json_object();
    categories = json_array();
    info = &reply->bc;

    /* Encode categories */
    nelems = reply->nelems;
    for (i = 0; i < nelems; i++)
    {
        int confidence_level;
        json_t *category;
        int category_id;

        category = json_object();
        category_id = reply->categories[i];
        confidence_level = info->confidence_levels[i];
        /* Encode category and confidence */
        json_object_set_new(category, "categoryId",
                            json_integer(category_id));
        json_object_set_new(category, "confidenceLevel",
                            json_integer(confidence_level));

        /* Append category, confidence data to the categories array */
        json_array_append_new(categories, category);
    }

    /* Encode categorization */
    json_object_set_new(categorization, "reputationScore",
                        json_integer(info->reputation));
    json_object_set_new(categorization, "categories",
                        categories);


    return categorization;
}


/**
 * @brief encodes a FQDN report in json format
 *
 * @param session fsm session storing the header information
 * @param to_report URL information to report
 * @return a string containing the json encoded information
 *
 * The caller needs to free the string pointer through a json_free() call.
 */
char *
jencode_url_report(struct fsm_session *session,
                   struct fqdn_pending_req *to_report)
{
    struct fsm_url_request *url_info;
    struct fsm_url_reply *reply;
    char str_mac[128] = { 0 };
    bool report_categories;
    json_t *categorization;
    json_t *body_envelope;
    json_t *json_report;
    json_t *ipv4_addrs;
    json_t *ipv6_addrs;
    os_macaddr_t *mac;
    char *json_msg;
    json_t *body;
    bool ready;
    char *str;
    int i;

    ready = jcheck_header_info(session);
    if (ready == false) return NULL;

    url_info = to_report->req_info;
    json_report  = json_object();
    body_envelope = json_array();
    body = json_object();

    /* Encode header */
    jencode_header(session, json_report);

    /* Encode body */
    mac = &to_report->dev_id;
    snprintf(str_mac, sizeof(str_mac),
             PRI(os_macaddr_t), FMT(os_macaddr_pt, mac));
    str = str_mac;
    json_object_set_new(body, "deviceMac", json_string(str));
    str = url_info->url;
    json_object_set_new(body, "dnsAddress", json_string(str));
    str = get_action_str(to_report);
    json_object_set_new(body, "action", json_string(str));
    str = to_report->policy;
    json_object_set_new(body, "policy", json_string(str));
    json_object_set_new(body, "policyIndex",
                        json_integer(to_report->policy_idx));
    str = to_report->rule_name;
    json_object_set_new(body, "ruleName", json_string(str));

    /* Report categories if a categorization query was done and successful */
    report_categories = ((to_report->categorized != FSM_FQDN_CAT_NOP) &&
                         (to_report->categorized != FSM_FQDN_CAT_FAILED));

    reply = url_info->reply;

    if (report_categories == true)
    {
        char *no_provider = "webroot";
        int service_id;

        categorization = NULL;
        service_id = reply->service_id;
        if (service_id == URL_BC_SVC) categorization = jencode_bc_report(reply);
        if (service_id == URL_WP_SVC) categorization = jencode_wb_report(reply);

        /* Add categorization source */
        str = (to_report->provider != NULL ? to_report->provider : no_provider);
        json_object_set_new(categorization, "source",
                            json_string(str));

        /* Add categorization to the body */
        json_object_set_new(body, "dnsCategorization", categorization);
    }

    if (to_report->ipv4_cnt != 0)
    {
        ipv4_addrs = json_array();
        for (i = 0; i < to_report->ipv4_cnt; i++)
        {
            str = to_report->ipv4_addrs[i];
            json_array_append_new(ipv4_addrs, json_string(str));
        }
        json_object_set_new(body, "resolvedIPv4", ipv4_addrs);
    }
    if (to_report->ipv6_cnt != 0)
    {
        ipv6_addrs = json_array();
        for (i = 0; i < to_report->ipv6_cnt; i++)
        {
            str = to_report->ipv6_addrs[i];
            json_array_append_new(ipv6_addrs, json_string(str));
        }
        json_object_set_new(body, "resolvedIPv6", ipv6_addrs);
    }

    /* Report error failures */
    if (to_report->categorized == FSM_FQDN_CAT_FAILED)
    {
        json_object_set_new(body, "lookupError",
                            json_integer(reply->error));
        if (reply->lookup_status > 0)
        {
            json_object_set_new(body, "httpStatus",
                                json_integer(reply->lookup_status));
        }
    }

    /* Encode body envelope */
    json_array_append_new(body_envelope, body);
    json_object_set_new(json_report, "dnsQueries", body_envelope);

    /* Convert json object in a compact string */
    json_msg = json_dumps(json_report, JSON_COMPACT);
    json_decref(json_report);

    return json_msg;
}


/**
 * @brief encodes a upnp report in json format
 *
 * @param session fsm session storing the header information
 * @param to_report upnp information to report
 * @return a string containing the json encoded information
 *
 * The caller needs to free the string pointer through a json_free() call.
 */
char *
jencode_upnp_report(struct fsm_session *session,
                    struct upnp_report *to_report)
{
    struct upnp_key_val *elem;
    char str_mac[128] = { 0 };
    json_t *body_envelope;
    json_t *json_report;
    os_macaddr_t *mac;
    char *json_msg;
    json_t *body;
    int nelems;
    bool ready;
    char *str;
    int i;

    ready = jcheck_header_info(session);
    if (ready == false) return NULL;

    json_report  = json_object();
    body_envelope = json_array();
    body = json_object();

    /* Encode header */
    jencode_header(session, json_report);

    /* Encode body */
    mac = &to_report->url->udev->device_mac;
    snprintf(str_mac, sizeof(str_mac),
             PRI(os_macaddr_t), FMT(os_macaddr_pt, mac));
    str = str_mac;
    json_object_set_new(body, "deviceMac", json_string(str));
    nelems = to_report->nelems;
    elem = to_report->first;
    for (i = 0; i < nelems; i++)
    {
        char *label = elem->key;

        str = elem->value;
        elem++;
        if ((strlen(str) != 0) && (strlen(label) != 0))
        {
            json_object_set_new(body, label, json_string(str));
        }
    }

    /* Encode body envelope */
    json_array_append_new(body_envelope, body);
    json_object_set_new(json_report, "upnpInfo", body_envelope);

    /* Convert json object in a compact string */
    json_msg = json_dumps(json_report, JSON_COMPACT);
    json_decref(json_report);

    return json_msg;
}


/**
 * @brief encodes a dhcp report in json format
 *
 * @param session fsm session storing the header information
 * @param to_report dhcp information to report
 * @return a string containing the json encoded information
 *
 * The caller needs to free the string pointer through a json_free() call.
 */
char *
jencode_dhcp_report(struct fsm_session *session,
                    struct dhcp_report *to_report)
{
    struct dhcp_local_domain *domain;
    json_t *body_envelope;
    json_t *json_report;
    json_t *domains;
    ds_tree_t *tree;
    char *json_msg;
    json_t *body;
    bool ready;
    char *str;

    ready = jcheck_header_info(session);
    if (ready == false) return NULL;

    json_report = json_object();
    body_envelope = json_array();
    body = json_object();

    jencode_header(session, json_report);
    tree = to_report->domain_list;
    domain = ds_tree_head(tree);
    domains = json_array();
    while (domain != NULL)
    {
        str = domain->name;
        json_array_append_new(domains, json_string(str));
        domain = ds_tree_next(tree, domain);
    }

    json_object_set_new(body, "localDomainNames", domains);
    json_array_append_new(body_envelope, body);
    json_object_set_new(json_report, "dhcpInfo", body_envelope);
    /* Convert json object in a compact string */
    json_msg = json_dumps(json_report, JSON_COMPACT);
    json_decref(json_report);

    return json_msg;
}
