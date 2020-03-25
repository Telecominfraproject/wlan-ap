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

#ifndef OSN_UPNP_H_INCLUDED
#define OSN_UPNP_H_INCLUDED

/**
 * @file osn_upnp.h
 * @brief OpenSync UPnP
 *
 * @addtogroup OSN
 * @{
 *
 * @addtogroup OSN_IPV4
 * @{
 *
 * @defgroup OSN_UPNP UPnP
 *
 * UPnP API definitions.
 *
 * @{
 */

/*
 * ===========================================================================
 * UPnP driver interface
 * ===========================================================================
 */

#include "osn_types.h"

/**
 * @struct osn_upnp
 *
 * OSN UPnP service object. The actual structure implementation is hidden
 * and is platform dependent. A new instance of the object can be obtained by
 * calling @ref osn_upnp_new() and must be destroyed using @ref
 * osn_upnp_del().
 */
typedef struct osn_upnp osn_upnp_t;

/**
 * UPNP MODE, specifies the mode of the interface:
 *
 * - UPNP_MODE_NONE     - None, this interface is not participating in any
 *                        UPnP configuration
 * - UPNP_MODE_INTERNAL - This interface is a UPnP LAN facing interface
 * - UPNP_MODE_EXTERNAL - This interface is a UPnP WAN facing interface
 */
enum osn_upnp_mode
{
    UPNP_MODE_NONE,         /**< Default, no UPnP settings */
    UPNP_MODE_INTERNAL,     /**< This is the LAN facing UPnP interface */
    UPNP_MODE_EXTERNAL,     /**< This is the WAN facing UPnP interface */
};


/**
 * Create a new instance of a UPnP object.
 *
 * @param[in]   ifname  Interface name to which the UPnP instance will be bound
 *
 * @return
 * This function returns NULL if an error occurs, otherwise a valid @ref
 * osn_upnp_t object is returned.
 */
osn_upnp_t *osn_upnp_new(const char *ifname);

/**
 * Destroy a valid osn_upnp_t object.
 *
 * @param[in]   self  A valid pointer to an osn_upnp_t object
 *
 * @return
 * This function returns true on success. On error, false is returned.
 * The input parameter should be considered invalid after this function
 * returns, regardless of the error code.
 */
bool osn_upnp_del(osn_upnp_t *self);

/**
 * Start a UPnP server instance
 *
 * @param[in]   self  A valid pointer to an osn_upnp_t object
 *
 * @return
 * This function returns false if the service was unable to be started.
 *
 * @note
 * osn_upnp_start()/osn_upnp_stop() will be replaced with a single call
 * to osn_upnp_apply() in the next release.
 */
bool osn_upnp_start(osn_upnp_t *self);

/**
 * Stop a UPnP server instance
 *
 * @param[in]   self  A valid pointer to an osn_upnp_t object
 *
 * @return
 * This function returns false if the service was successfully stopped.
 *
 * @note
 * osn_upnp_start()/osn_upnp_stop() will be replaced with a single call
 * to osn_upnp_apply() in the next release.
 */
bool osn_upnp_stop(osn_upnp_t *self);

/**
 * Sets the UPnP mode on the current interface.
 *
 * @param[in]   self  A valid pointer to an osn_upnp_t object
 * @param[in]   mode  The UPnP mode for this interface as described in @ref osn_upnp_mode
 */
bool osn_upnp_set(osn_upnp_t *self, enum osn_upnp_mode mode);

/**
 * Gets the current UPnP mode on the current interface.
 *
 * @param[in]   self  A valid pointer to an osn_upnp_t object
 * @param[out]  mode  The UPnP mode for this interface as described in @ref osn_upnp_mode
 */
bool osn_upnp_get(osn_upnp_t *self, enum osn_upnp_mode *upnp_mode);

/** @} OSN_UPNP */
/** @} OSN_IPV4 */
/** @} OSN */

#endif /* OSN_UPNP_H_INCLUDED */
