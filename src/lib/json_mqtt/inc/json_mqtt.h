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

#ifndef __JSON_MQTT_H__
#define __JSON_MQTT_H__

#include <jansson.h>

#include "http_parse.h"
#include "upnp_parse.h"
#include "fsm_policy.h"
#include "dhcp_parse.h"

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
                   struct http_parse_report *to_report);


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
                   struct fqdn_pending_req *to_report);



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
                    struct upnp_report *to_report);



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
                    struct dhcp_report *to_report);

#endif /* __JSON_MQTT_H__ */
