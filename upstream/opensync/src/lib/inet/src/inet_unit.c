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

#include <stdarg.h>
#include <string.h>

#include "log.h"

#include "inet_unit.h"

/*
 * ===========================================================================
 *  Library for managing units - a tree-like structure of dependencies.
 *
 *  This is primarily used to manage dependencies of interface network
 *  configuration and associated services. (For example, firewall must be started
 *  before the interface is enabled...
 * ===========================================================================
 */

/**
 * Initialize a unit, inet_unit() calls can be chained inside other
 * inet_unit() calls which makes it somewhat convenient (if somewhat
 * unreadable) to declare the dependency tree structures. Make sure to
 * always terminate the argument list with NULL.
 */
inet_unit_t *__inet_unit(intptr_t unit_id, bool started, inet_unit_t *dep, va_list va)
{
    va_list ap;
    inet_unit_t *ru;
    inet_unit_t *pu;

    ru = calloc(1, sizeof(inet_unit_t));
    ru->un_id = unit_id;

    ru->un_status = started;
    ru->un_pending = started;

    /* Start unit by default */
    ru->un_enabled = false;

    /* Count the number of elements */
    ru->un_ndeps = 0;
    va_copy(ap, va);
    for (pu = dep; pu != NULL; pu = va_arg(ap, inet_unit_t *))
    {
        ru->un_ndeps++;
    }
    va_end(ap);

    /* Allocate enough space to build the list of elements + the NULL terminator */
    if (ru->un_ndeps == 0)
    {
        ru->un_deps = NULL;
    }
    else
    {
        ru->un_deps = calloc(ru->un_ndeps, sizeof(inet_unit_t *));
    }

    /* Build and return the list of dependencies */
    inet_unit_t **du;

    du = ru->un_deps;
    va_copy(ap, va);
    for (pu = dep; pu != NULL; pu = va_arg(ap, inet_unit_t *))
    {
        pu->un_parent = ru;
        *du++ = pu;
    }
    va_end(ap);

    return ru;
}

inet_unit_t *inet_unit(intptr_t unit_id, inet_unit_t *dep, ...)
{
    va_list va;

    inet_unit_t *rc;

    va_start(va, dep);

    rc = __inet_unit(unit_id, false, dep, va);

    va_end(va);

    return rc;
}

/**
 * Same as inet_unit(), except it tells that the unit is enabled by default.
 */
inet_unit_t *inet_unit_s(intptr_t unit_id, inet_unit_t *dep, ...)
{
    va_list va;

    inet_unit_t *rc;

    va_start(va, dep);

    rc = __inet_unit(unit_id, true, dep, va);

    va_end(va);

    return rc;
}


/**
 * Traverse the unit dependency list
 */
bool inet_unit_walk(inet_unit_t *unit, inet_unit_walk_fn_t *fn, void *ctx)
{
    size_t ii;

    bool retval = true;

    /* Do not descend if the entry function fails */
    if (!fn(unit, ctx, true)) return false;

    for (ii = 0; ii < unit->un_ndeps; ii++)
    {
        retval &= inet_unit_walk(unit->un_deps[ii], fn, ctx);
    }

    if (retval)
    {
        retval &= fn(unit, ctx, false);
    }

    return retval;
}

/**
 * Free memory used by the unit and all subunits
 */
bool __inet_unit_free(inet_unit_t *unit, void *ctx, bool descend)
{
    if (!descend)
    {
        memset(unit->un_deps, 0, sizeof(unit->un_deps[0]) * unit->un_ndeps);
        free(unit->un_deps);

        memset(unit, 0, sizeof(*unit));
        free(unit);
    }

    return true;
}

void inet_unit_free(inet_unit_t *unit)
{
    inet_unit_walk(unit, __inet_unit_free, NULL);
}

/**
 * Lookup a unit by its id
 */
struct inet_unit_find_args
{
    intptr_t find;
    inet_unit_t *result;
};

bool __inet_unit_find(inet_unit_t *unit, void *ctx, bool descend)
{
    struct inet_unit_find_args *args = ctx;

    if (!descend) return true;

    if (unit->un_id == args->find)
    {
        args->result = unit;
        return false;   /* Break the walk */
    }

    return true;
}

inet_unit_t *inet_unit_find(inet_unit_t *unit, intptr_t id)
{
    struct inet_unit_find_args args =
    {
        .find = id,
        .result = NULL,
    };


    if (inet_unit_walk(unit, __inet_unit_find, &args))
    {
        return NULL;
    }

    return args.result;
}

bool inet_unit_start(inet_unit_t *unit, intptr_t unit_id)
{
    inet_unit_t *pu = NULL;

    pu = inet_unit_find(unit, unit_id);
    if (pu == NULL)
    {
        /* Not found */
        return false;
    }

    pu->un_enabled = true;

    return true;
}

bool __inet_unit_stop(inet_unit_t *unit, void *ctx, bool descend)
{
    if (descend) unit->un_pending = true;

    return true;
}

bool inet_unit_stop(inet_unit_t *unit, intptr_t unit_id)
{
    inet_unit_t *pu = NULL;

    pu = inet_unit_find(unit, unit_id);
    if (pu == NULL)
    {
        /* Not found */
        return false;
    }

    pu->un_enabled = false;

    inet_unit_walk(pu, __inet_unit_stop, NULL);

    return true;
}

/**
 * Restart a unit (issue a stop event, and then a start event). If @p start_if_stopped is false
 * and the unit is already stopped, do nothing.
 */
bool inet_unit_restart(inet_unit_t *unit, intptr_t unit_id, bool start_if_stopped)
{
    inet_unit_t *pu = NULL;

    pu = inet_unit_find(unit, unit_id);
    if (pu == NULL)
    {
        /* Not found */
        return false;
    }

    if (start_if_stopped || pu->un_enabled)
    {
        /* Stop the service only if it is already enabled */
        if (!inet_unit_stop(unit, unit_id)) return false;

        /* Restart it */
        if (!inet_unit_start(unit, unit_id)) return false;
    }

    return true;
}

/**
 * Start or stop unit according to the variable @p enable
 */
bool inet_unit_enable(inet_unit_t *unit, intptr_t unit_id, bool enable)
{
    bool retval;

    if (enable)
    {
        retval = inet_unit_start(unit, unit_id);
    }
    else
    {
        retval = inet_unit_stop(unit, unit_id);
    }

    return retval;
}

/**
 * Commit pending changes
 */
struct __inet_unit_commit_args
{
    inet_unit_commit_fn_t   *fn;
    void                    *fn_ctx;
};

bool __inet_unit_commit_start(inet_unit_t *unit, void *ctx, bool descend)
{
    struct __inet_unit_commit_args *args = ctx;

    /* Start units when descending into the dependency tree */
    if (!descend) return true;

    /* If the parent unit is not started, this one shouldn't be either */
    if (unit->un_parent != NULL)
    {
        if (!unit->un_parent->un_status) return true;
    }

    if (!unit->un_enabled || unit->un_status) return true;

    /* Start the unit */
    unit->un_error = !args->fn(args->fn_ctx, unit->un_id, true);
    unit->un_status = true;
    unit->un_pending = false;

    if (unit->un_error) return false;

    return true;
}

bool __inet_unit_commit_stop(inet_unit_t *unit, void *ctx, bool descend)
{
    struct __inet_unit_commit_args *args = ctx;

    /* Stop units only when ascending from the dependency tree */
    if (descend) return true;

    if (!unit->un_pending && unit->un_enabled) return true;

    /* Stop the unit only if it is started */
    if (unit->un_status)
    {
        unit->un_error = !args->fn(args->fn_ctx, unit->un_id, false);
    }

    unit->un_pending = false;
    unit->un_status = false;

    return true;
}

bool inet_unit_commit(inet_unit_t *unit, inet_unit_commit_fn_t *fn, void *fn_ctx)
{
    bool retval = true;

    struct __inet_unit_commit_args args;

    args.fn = fn;
    args.fn_ctx = fn_ctx;

    /* Stop units first */
    retval &= inet_unit_walk(unit, __inet_unit_commit_stop, &args);
    /* Start units */
    retval &= inet_unit_walk(unit, __inet_unit_commit_start, &args);

    return retval;
}

/**
 * Returns true if unit is enabled
 */
bool inet_unit_status(inet_unit_t *unit, intptr_t unit_id)
{
    inet_unit_t *pu = inet_unit_find(unit, unit_id);

    if (pu == NULL) return false;

    return pu->un_status;
}

/**
 * Returns true if unit is enabled
 */
bool inet_unit_is_enabled(inet_unit_t *unit, intptr_t unit_id)
{
    inet_unit_t *pu = inet_unit_find(unit, unit_id);

    if (pu == NULL) return false;

    return pu->un_enabled;
}
