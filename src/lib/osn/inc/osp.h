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

#if !defined(OSP_H_INCLUDED)
#define OSP_H_INCLUDED

#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>


/**
 * LED API
 */
enum osp_led_mode
{
    OSP_LED_MODE_OFF,
    OSP_LED_MODE_ON,
    OSP_LED_MODE_BREATHE,
    OSP_LED_MODE_PATTERN,
    OSP_LED_MODE_BLINK,
    OSP_LED_MODE_MORSE,
};

#define OSP_LED_RGB         (3)
#define OSP_LED_COLORS      (OSP_LED_RGB + 1)
#define OSP_LED_PATTERN_MAX (8)


enum osp_led_state
{
    OSP_LED_ST_IDLE,
    OSP_LED_ST_ERROR,
    OSP_LED_ST_CONNECT,
    OSP_LED_ST_CONNECTING,
    OSP_LED_ST_CONNECTFAIL,
    OSP_LED_ST_WPS,
    OSP_LED_ST_OPTIMIZE,
    OSP_LED_ST_LOCATE,
    OSP_LED_ST_HWERROR,
    OSP_LED_ST_BTCONNECTING,
    OSP_LED_ST_BTCONNECT,
    OSP_LED_ST_UPGRADING,
    OSP_LED_ST_UPGRADEFAIL,
    OSP_LED_ST_THERMAL,
    OSP_LED_ST_LAST,
};


struct osp_led_params
{
    /* LED intensity */
    int intensity[OSP_LED_COLORS];

    union
    {
        /* extra parameters for pattern LED mode;
         * each element of the array specifies how much time in milliseconds
         * the LED will be turned on or off */
        uint16_t pattern[OSP_LED_PATTERN_MAX];

        struct
        {
            /* extra parameters for blinking LED mode;
             * blinking periods in milliseconds
             */
            int delay_on;
            int delay_off;
        } blink;

        /* Morse code message text including \0 */
        char message[32];

    } ledmode;
};


/**
 * Initialize LED subsystem.
 *
 * @param led_cnt pointer where number of LED's supported on system will be stored
 *
 * @return 0 on success, -1 on error
 */
int osp_led_init(int *led_cnt);


/**
 * Set LED to specified high level state (high-level LED API)
 *
 * @param state     business logic level LED state implemented by target layer
 * @param priority  LED zero index priority - 0 is highest. Higher priority
 *                  state changes overrides current LED behavior
 *
 * @return 0 on success, -1 on error
 */
int osp_led_set_state(enum osp_led_state state, uint32_t priority);


/**
 * Clear current state. If state is of highest priority, lower priority state
 * is to be applied. If there are no states on LED state stack, LED IDLE state
 * is to be applied
 *
 * @param state     business logic level LED state implemented by target layer
 *
 * @return 0 on success, -1 on error
 */
int osp_led_clear_state(enum osp_led_state state);


/**
 * Set LED to IDLE state and clear all LED states history
 *
 * @return 0 on success, -1 on error
 */
int osp_led_reset();


/**
 * get LED current state
 *
 * @return 0 on success, -1 on error
 */
int osp_led_get_state(enum osp_led_state * state, uint32_t * priority);


/**
 * Set LED to specified mode (low level LED API)
 *
 * @param led_id index of the LED; index starts with 0
 * @param mode LED mode to be applied
 * @param params pointer to extra parameters for specific LED modes
 *
 * @return 0 on success, -1 on error
 */
int osp_led_set_mode(int led_id, enum osp_led_mode mode, struct osp_led_params *params);


/*
 * Button API
 */

/**
 * Enumeration of buttons supported by OpenSync
 */
enum osp_btn_name
{
    /**
     * @brief Button name for the factory reset button
     */
    OSP_BTN_NAME_RESET = (1 << 0),

    /**
     * @brief Button name for the WiFi WPS button
     */
    OSP_BTN_NAME_WPS = (1 << 1),

    /* More buttons can be added after */
};

/**
 * @brief Get the capabilities related to the buttons
 * @param[out] caps Bitmask of buttons supported by the target
 *                  You can test if a button is supported by testing the bitmask
 *                  For example, to test if the reset button is supported by the
 *                  target, you can test (caps & OSP_BTN_NAME_RESET)
 * @return true on success
 */
bool osp_btn_get_caps(uint32_t *caps);

/**
 * @brief Definition of an event associated to the button
 *
 * Example 1: Button is pushed
 *            - pushed = true
 *            - duration = 0
 *            - double_click = false
 *
 * Example 2: Button is double click
 *            - pushed = false
 *            - duration = 0
 *            - double_click = true
 *
 * Example 3: Button is release after 1 second
 *            - pushed = false
 *            - duration = 1000
 *            - double_click = false
 *
 * Example 4: Button is release after 5 second
 *            - pushed = false
 *            - duration = 5000
 *            - double_click = false
 */
struct osp_btn_event
{
    /**
     * @brief true if the button is pushed, false is the button is released
     */
    bool pushed;

    /**
     * @brief Duration in milliseconds of pressing the button
     *        Valid only when the button is released and when this is not a double click
     */
    unsigned int duration;

    /**
     * @brief true if the button is pushed and released two times in less than 1000 milliseconds
     *        Valid only when the button is released
     */
    bool double_click;
};

/**
 * @brief Callback called by the target layer when an event is received on a button
 * @param[in] obj Pointer on the object which was given at target_button_register() call
 * @param[in] name Button associated to the event
 * @param[in] event Details of the button event
 */
typedef void (*osp_btn_cb)(void *obj, enum osp_btn_name name, const struct osp_btn_event *event);

/**
 * @brief Register the callback to receive button events
 * @param[in] cb Callback called by the target layer when an event is received
 *               on a button
 *               If callback is NULL, the target must unregister the previous
 *               one for this specific obj
 * @param[in] obj User pointer which will be given back when the callback will
 *                be called
 * @return true on success
 */
bool osp_btn_register(osp_btn_cb cb, void *obj);

/**
 * @brief Unit reboot
 * @param[in] reason Reboot request source - to be logged
 * @param[in] ms_delay Delay actual reboot in ms
 *
 * @return true on success
 */
bool osp_unit_reboot(const char *reason, int ms_delay);

/**
 * @brief Unit reboot & factory reset
 * @param[in] reason Reboot request source - to be logged
 * @param[in] ms_delay Delay actual factory reboot in ms
 *
 * @return true on success
 */
bool osp_unit_factory_reboot(const char *reason, int ms_delay);


/*
 * FW Upgrade API
 */

/**
 * @brief Type of upgrade operations
 */
typedef enum
{
    OSP_UPG_DL,     /* Download upgrade file    */
    OSP_UPG_DL_CS,  /* Download checksum file   */
    OSP_UPG_UPG     /* Upgrade process          */
} osp_upg_op_t;


/**
 * @brief Upgrade operations status
 */
typedef enum
{
    OSP_UPG_OK           = 0,   /* success                       */
    OSP_UPG_ARGS         = 1,   /* Wrong arguments (app error)   */
    OSP_UPG_URL          = 3,   /* error setting url             */
    OSP_UPG_DL_FW        = 4,   /* DL of FW image failed         */
    OSP_UPG_DL_MD5       = 5,   /* DL of *.md5 sum failed        */
    OSP_UPG_MD5_FAIL     = 6,   /* md5 CS failed or platform     */
    OSP_UPG_IMG_FAIL     = 7,   /* image check failed            */
    OSP_UPG_FL_ERASE     = 8,   /* flash erase failed            */
    OSP_UPG_FL_WRITE     = 9,   /* flash write failed            */
    OSP_UPG_FL_CHECK     = 10,  /* flash verification failed     */
    OSP_UPG_BC_SET       = 11,  /* new FW commit failed          */
    OSP_UPG_APPLY        = 12,  /* Applying new FW failed        */
    OSP_UPG_BC_ERASE     = 14,  /* clean FW commit info failed   */
    OSP_UPG_SU_RUN       = 15,  /* upgrade in progress running   */
    OSP_UPG_DL_NOFREE    = 16,  /* Not enough free space on unit */
    OSP_UPG_WRONG_PARAM  = 17,  /* Wrong flashing parameters     */
    OSP_UPG_INTERNAL     = 18   /* Internal error                */
} osp_upg_status_t;


/**
 * @brief Callback invoked by target layer during download & upgrade process
 * @param[in] op - operation: download, download CS file or upgrade
 * @param[in] status status
 * @param[in] completed percentage of completed work 0 - 100%
 */
typedef void (*osp_upg_cb)(const osp_upg_op_t op,
                           const osp_upg_status_t status,
                           uint8_t completed);


/**
 * @brief Check system requirements for upgrade, like
 *        no upgrade in progress, available flash space etc
 */
bool osp_upg_check_system(void);

/**
 * Download an image suitable for upgrade from @p uri store it locally.
 * Upon download and verification completion, invoke the @p dl_cb callback.
 */
bool osp_upg_dl(char * url, uint32_t timeout, osp_upg_cb dl_cb);

/**
 * Write the previously downloaded image to the system. If the image
 * is encrypted, a password must be specified in @password.
 *
 * After the image was successfully applied, the @p upg_cb callback is invoked.
 */
bool osp_upg_upgrade(char *password, osp_upg_cb upg_cb);

/**
 * On dual-boot system, flag the newly flashed image as the active one.
 * This can be a no-op on single image systems.
 */
bool osp_upg_commit(void);

/**
 * Activate to the new image (must be called after a osp_upg_commit()).
 * This implies a reboot of the system.
 */
bool osp_upg_apply(uint32_t timeout_ms);

/**
 * Return more detailed error code in relation to a failed osp_upg_() function.
 * See osp_upg_status_t for a detailed list of error codes.
 */
int osp_upg_errno(void);

/**
 * Persistent storage API
 */

/**
 * Initialize Persistent Storage.
 *
 * @return true on success, false on failure
 */
bool osp_ps_init(void);


/**
 * Write changes to the persistent storage
 *
 * @return true on success, false on failure
 */
bool osp_ps_commit(void);


/**
 * Set a new key-value pair to the persistent storage. If the key already exists
 * in the storage, it will be overwritten.
 *
 * @param key
 * @param val
 *
 * @return
 */
bool osp_ps_set(const char *key, const char *val);

/**
 * Get the value for the requested key. The returned value can be truncated
 * if length of the provided buffer is less than the length of stored value.
 *
 * @param key
 * @param val buffer where value will be stored
 * @param len length of val buffer
 *
 * @return length of the stored value or return negative value on error
 */
int osp_ps_get(const char *key, char *val, const unsigned int len);

/**
 * Check if given key exists in the persistent storage
 *
 * @param key
 *
 * @return true if key exists, false otherwise
 */
bool osp_ps_exists(const char *key);

/**
 * Delete the given key from the persistent storage
 *
 * @return false on error or key still exists on function return
 */
bool osp_ps_delete(const char *key);


#endif /* OSP_H_INCLUDED */
