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

#ifndef KCONFIG_H_INCLUDED
#define KCONFIG_H_INCLUDED

/*
 * Note: The code below is inspired by include/linux/kconfig.h from the Linux kernel.
 *
 * To explain the code below, lets assume we have two cases:
 *
 * #define CONFIG_A 1
 * or
 * #undef CONFIG_A
 *
 * step 0) Now, if we call kconfig_enabled(CONFIG_A) there are two possible outcomes:
 *
 *  - the first case will expand to kconfig_enabled0(1)
 *  - the second case will expand to kconfig_enabled0(CONFIG_A)
 *
 * step 1) Now, lets look what happens with kconfig_enabled0()
 *
 * - kconfig_enabled0(1) gets expanded to           -> kconfig_enabled1(kconfig_junk_1, 0)
 * - kconfig_enabled0(CONFIG_A) gets expanded to    -> kconfig_enabled1(kconfig_junk_CONFIG_A, 0)
 *
 * step 2) Expansion of kconfig_enabled1(). Note that we defined kconfig_junk_1 to be "0, 1"
 *
 * - kconfig_enabled1(kconfig_junk_1, 0) is expanded to         -> kconfig_enabled2(junk, 1, 0)
 * - kconfig_enabled1(kconfig_junk_CONFIG_A, 0) is expanded to  -> kconfig_enabled2(kconfig_junk_CONFIG_A, 0)
 *
 * step 3) Due to the expansios above, kconfig_enabled2() may be called with 2 or 3 arguments -- we always return the second
 *
 * - kconfig_enabled2(junk, 1, 0)               -> 1
 * - kconfig_enabled2(kconfig_junk_CONFIG_A, 0) -> 0
 */

#define kconfig_junk_1 junk, 1
#define kconfig_enabled(KC)         kconfig_enabled0(KC)
#define kconfig_enabled0(KC)        kconfig_enabled1(kconfig_junk_##KC, 0)
#define kconfig_enabled1(...)       kconfig_enabled2(__VA_ARGS__)
#define kconfig_enabled2(A, B, ...) B

#endif /* KCONFIG_H_INCLUDED */

