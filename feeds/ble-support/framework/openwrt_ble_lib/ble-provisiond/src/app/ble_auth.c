/*
 * Copyright © Accton Technology Corporation. All rights reserved.
 *
 * @file ble_auth.c
 * @brief Application-layer authentication for GATT provisioning v2.
 *
 * See ble_auth.h and DESIGN_OVERVIEW §19.4. Mirrors the reference phone
 * app's behaviour: verify a plaintext credential against the /etc/shadow
 * root hash with crypt_r(), fail-closed, re-read every call.
 */

#define _GNU_SOURCE

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <syslog.h>
#include <crypt.h>

#include "ble_auth.h"

/*
 * No authentication state here — see the note in ble_auth.h. It is kept per link
 * in gatt_v2_conn.c, because a single flag shared by every connection let one
 * client's successful authentication grant another client full control.
 */

/*
 * Read the root password hash field from /etc/shadow.
 * Copies the 2nd colon-separated field (the hash) into @out.
 * Returns true if a root row was found (even if the hash is locked/empty).
 */
static bool read_root_shadow_hash(char *out, size_t out_size)
{
    FILE *fp = fopen("/etc/shadow", "r");
    if (!fp)
        return false;

    char line[512];
    bool found = false;
    while (fgets(line, sizeof(line), fp)) {
        if (strncmp(line, "root:", 5) != 0)
            continue;

        /* Field 1 (0-based) is the hash: root:<hash>:<...> */
        const char *p = line + 5;                 /* after "root:" */
        const char *end = strchr(p, ':');
        size_t len = end ? (size_t)(end - p) : strlen(p);
        /* strip trailing newline if no second colon */
        while (len > 0 && (p[len - 1] == '\n' || p[len - 1] == '\r'))
            len--;
        if (len >= out_size)
            len = out_size - 1;
        memcpy(out, p, len);
        out[len] = '\0';
        found = true;
        break;
    }

    fclose(fp);
    return found;
}

/**
 * The hash scheme of @stored: "$1$", "$5$", "$6$", "$y$", "des", …
 *
 * Safe to log. The scheme names the algorithm, not the secret; the salt and
 * digest that follow it are deliberately dropped. Without this, an unsupported
 * scheme is indistinguishable from a wrong password, which is exactly the hole
 * that made an eap115 auth failure undiagnosable from syslog.
 */
static void hash_scheme(const char *stored, char *out, size_t out_size)
{
    if (!out || out_size == 0)
        return;

    if (stored[0] != '$') {
        snprintf(out, out_size, "des");
        return;
    }

    const char *close = strchr(stored + 1, '$');
    if (!close) {
        snprintf(out, out_size, "malformed");
        return;
    }

    size_t len = (size_t)(close - stored) + 1;   /* through the closing '$' */
    if (len >= out_size)
        len = out_size - 1;
    memcpy(out, stored, len);
    out[len] = '\0';
}

/**
 * Flag file that opts in to dumping a rejected credential.
 *
 * A file rather than a UCI option, and in tmpfs rather than /etc: enabling it is
 * `touch`, disabling it is `rm` or a reboot, and there is no way for it to be
 * left switched on in a shipped image. It exists because a rejected credential is
 * otherwise invisible — the daemon can prove that the password it was given is
 * wrong, but not *what* it was given, and when the phone app is the other half of
 * the contract that is the only question left.
 */
#define BLE_AUTH_DEBUG_FLAG  "/tmp/ble-auth-debug"

/**
 * Log a rejected credential as hex, but only when the flag file is present.
 *
 * Hex, not the string: the bytes may not be printable, and a truncated or
 * re-encoded credential (UTF-16, a trailing newline, a length prefix) is exactly
 * the kind of thing worth being able to see. Only the *rejected* value is ever
 * logged — a credential that matched is a working password and stays out of the
 * log entirely.
 */
static void log_rejected_credential(const char *provided)
{
    if (access(BLE_AUTH_DEBUG_FLAG, F_OK) != 0)
        return;

    size_t n = strlen(provided);
    if (n > 64)
        n = 64;

    char hex[64 * 3 + 1];
    size_t w = 0;
    for (size_t i = 0; i < n; i++)
        w += (size_t)snprintf(hex + w, sizeof(hex) - w, "%02x ",
                              (unsigned char)provided[i]);
    if (w > 0 && hex[w - 1] == ' ')
        hex[w - 1] = '\0';

    syslog(LOG_WARNING, "ble_auth: [%s present] rejected credential was: %s",
           BLE_AUTH_DEBUG_FLAG, hex);
}

bool ble_auth_verify_root_password(const char *provided)
{
    if (!provided || provided[0] == '\0') {
        syslog(LOG_WARNING, "ble_auth: empty credential — refused");
        return false;   /* fail-closed: empty credential */
    }

    char stored[256];
    if (!read_root_shadow_hash(stored, sizeof(stored))) {
        syslog(LOG_ERR, "ble_auth: no usable root row in /etc/shadow — every "
               "credential will be refused");
        return false;
    }

    /* Locked / disabled / no password set → refuse everything. */
    if (stored[0] == '\0' ||
        strcmp(stored, "*")  == 0 ||
        strcmp(stored, "!")  == 0 ||
        strcmp(stored, "!!") == 0) {
        syslog(LOG_ERR, "ble_auth: the root account has no usable password "
               "(locked or unset) — every credential will be refused");
        return false;
    }

    char scheme[16];
    hash_scheme(stored, scheme, sizeof(scheme));

    /* crypt_r is thread-safe and reentrant; struct crypt_data must be
     * zero-initialised before first use. */
    struct crypt_data cd;
    memset(&cd, 0, sizeof(cd));

    const char *hash = crypt_r(provided, stored, &cd);

    /*
     * Two different failure conventions have to be caught here. glibc returns
     * NULL for a salt it cannot parse; musl — which is what OpenWrt builds
     * against — returns a failure token beginning with '*' instead. A stored
     * hash starting with '*' was already rejected above, so a '*' coming back
     * out can only mean crypt() gave up.
     *
     * Getting this wrong is not cosmetic: an unsupported scheme then looks
     * exactly like a mistyped password, and no password can ever succeed.
     */
    if (!hash || hash[0] == '*') {
        syslog(LOG_ERR, "ble_auth: crypt() cannot handle the root hash scheme "
               "'%s' on this build — NO credential can match. Re-hash the root "
               "password with a scheme libcrypt supports ($1$/$5$/$6$).",
               scheme);
        return false;
    }

    if (strcmp(hash, stored) != 0) {
        /* Length only, never the credential itself: it is enough to tell "the
         * app sent something short/long" apart from "the app sent nothing". */
        syslog(LOG_WARNING, "ble_auth: credential mismatch — %zu bytes checked "
               "against a '%s' root hash", strlen(provided), scheme);
        log_rejected_credential(provided);
        return false;
    }

    return true;
}

int ble_auth_get_mac(char *out, size_t out_size)
{
    if (!out || out_size < 13)
        return 0;

    FILE *fp = fopen("/sys/class/net/eth0/address", "r");
    if (!fp)
        return 0;

    char buf[32] = {0};
    if (!fgets(buf, sizeof(buf), fp)) {
        fclose(fp);
        return 0;
    }
    fclose(fp);

    /* Strip separators, lowercase, keep hex digits only. e.g.
     * "94:EF:97:D9:38:D0\n" -> "94ef97d938d0" */
    size_t n = 0;
    for (const char *p = buf; *p && n < out_size - 1; p++) {
        char c = *p;
        if (c == ':' || c == '-' || c == '\n' || c == '\r')
            continue;
        if (c >= 'A' && c <= 'F')
            c = (char)(c - 'A' + 'a');
        out[n++] = c;
    }
    out[n] = '\0';
    return (int)n;
}
