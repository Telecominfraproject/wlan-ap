/*
* Copyright (c) 2019, Sagemcom.
* All rights reserved.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are met:
*
* 1. Redistributions of source code must retain the above copyright notice,
*    this list of conditions and the following disclaimer.
*
* 2. Redistributions in binary form must reproduce the above copyright notice,
*    this list of conditions and the following disclaimer in the documentation
*    and/or other materials provided with the distribution.
*
* 3. Neither the name of the copyright holder nor the names of its contributors
*    may be used to endorse or promote products derived from this software
*    without specific prior written permission.
*
* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
* AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
* IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
* ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
* LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
* CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
* SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
* INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
* CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
* ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
* POSSIBILITY OF SUCH DAMAGE.
*/

#if !defined(OSN_FW_H_INCLUDED)
#define OSN_FW_H_INCLUDED

#include <netinet/in.h>
#include <stdbool.h>
#include <stdint.h>

/*
 * Iptables table enumeration
 */
enum osfw_table
{
    OSFW_TABLE_FILTER,
    OSFW_TABLE_NAT,
    OSFW_TABLE_MANGLE,
    OSFW_TABLE_RAW,
    OSFW_TABLE_SECURITY
};

/*
 * Initialize global firewall subsystem
 *
 * This function should set the default policy of the filter table's builtin
 * chains to DROP, and set the default policy to other tables to ACCEPT.
 * This function may also take care to clean the firewall subsystem, if it was
 * previously configured.
 */
bool osfw_init(void);

/*
 * Deinitialize the firewall subsystem; clean up etc.
 */
bool osfw_fini(void);

/*
 * Add a chain to the system:
 *      - family: AF_INET or AF_INET6
 *      - table: Table of the chain
 *      - chain: Name of the chain
 * The caller will not take any assomptions if the chain is a built-in chain
 * or not. For example, in the case of iptables, the caller can try to create
 * chain like "PREROUTING", "INPUT", "FORWARD", "OUTPUT" or "POSTROUTING". The
 * implementation has to manager properly these cases (it may just do nothing
 * and return true).
 * This is also true for built-in target, like "DROP", "ACCEPT" or "REJECT",
 * it is perfectly valid if the caller try to create a chain for these targets.
 * The implementation should also take care about these cases (it may just do
 * nothing and return true).
 * Exact list of built-in chains and built-in targets is implementation
 * specific.
 */
bool osfw_chain_add(int family, enum osfw_table table, const char *chain);

/*
 * Delete a chain from the system:
 *      - family: AF_INET or AF_INET6
 *      - table: Table of the chain
 *      - chain: Name of the chain
 * To delete a chain, the caller must remove first all rules inside this chain.
 */
bool osfw_chain_del(int family, enum osfw_table table, const char *chain);

/*
 * Add a rules in the system:
 *      - family: AF_INET or AF_INET6
 *      - table: Table of the rule
 *      - chain: The chain that the rule will be added to, must be created with
 *        osfw_chain_add() first
 *      - prio: Rule priority, 0 is the highest
 *      - match: A string using iptables match syntax
 *      - target: User defined chain or one of the target actions : "ACCEPT,
 *        DROP, REJECT"
 */
bool osfw_rule_add(int family, enum osfw_table table, const char *chain,
		int prio, const char *match, const char *target);

/*
 * Delete a rule from the system:
 *      - family: AF_INET or AF_INET6
 *      - table: Table of the rule
 *      - chain: The chain that the rule will be removed from
 *      - prio: Rule priority, 0 is the highest
 *      - match: A string using iptables match syntax
 *      - target: User defined chain or one of the target actions : "ACCEPT,
 *        DROP, REJECT"
 */
bool osfw_rule_del(int family, enum osfw_table table, const char *chain,
		int prio, const char *match, const char *target);

/*
 * Apply configuration to the system
 * The implementation should apply the configuration in the firewall subsystem
 * as defined by all previous calls to this API.
 */
bool osfw_apply(void);

#endif /* OSN_FW_H_INCLUDED */

