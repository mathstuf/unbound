/*
 * libunbound/context.h - validating context for unbound internal use
 *
 * Copyright (c) 2007, NLnet Labs. All rights reserved.
 *
 * This software is open source.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 
 * Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 * 
 * Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 * 
 * Neither the name of the NLNET LABS nor the names of its contributors may
 * be used to endorse or promote products derived from this software without
 * specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/**
 * \file
 *
 * This file contains the validator context structure.
 */
#ifndef LIBUNBOUND_CONTEXT_H
#define LIBUNBOUND_CONTEXT_H
#include "util/locks.h"
#include "util/alloc.h"
#include "util/rbtree.h"
#include "services/modstack.h"
#include "libunbound/unbound.h"

/**
 * The context structure
 *
 * Contains two pipes for async service
 *	qq : write queries to the async service pid/tid.
 *	rr : read results from the async service pid/tid.
 */
struct ub_val_ctx {
	/* --- pipes --- */
	/** mutex on query write pipe */
	lock_basic_t qqpipe_lock;
	/** the query write pipe, [0] read from, [1] write on */
	int qqpipe[2];
	/** mutex on result read pipe */
	lock_basic_t rrpipe_lock;
	/** the result read pipe, [0] read from, [1] write on */
	int rrpipe[2];

	/* --- shared data --- */
	/** mutex for access to env.cfg, finalized and dothread */
	lock_basic_t cfglock;
	/** 
	 * The context has been finalized 
	 * This is after config when the first resolve is done.
	 * The modules are inited (module-init()) and shared caches created.
	 */
	int finalized;

	/** do threading (instead of forking) for async resolution */
	int dothread;
	/** next thread number for new threads */
	int thr_next_num;
	/** 
	 * List of alloc-cache-id points per threadnum for notinuse threads.
	 * Simply the entire struct alloc_cache with the 'super' member used
	 * to link a simply linked list. Reset super member to the superalloc
	 * before use.
	 */
	struct alloc_cache* alloc_list;

	/** shared caches, and so on */
	struct alloc_cache superalloc;
	/** module env master value */
	struct module_env* env;
	/** module stack */
	struct module_stack mods;
	/** local authority zones */
	struct local_zones* local_zones;

	/** next query number (to try) to use */
	int next_querynum;
	/** number of async queries outstanding */
	size_t num_async;
	/** 
	 * Tree of outstanding queries. Indexed by querynum 
	 * Used when results come in for async to lookup.
	 * Used when cancel is done for lookup (and delete).
	 * Used to see if querynum is free for use.
	 * Content of type ctx_query.
	 */ 
	rbtree_t queries;
};

/**
 * The queries outstanding for the libunbound resolver.
 * These are outstanding for async resolution.
 * But also, outstanding for sync resolution by one of the threads that
 * has joined the threadpool.
 */
struct ctx_query {
	/** node in rbtree, must be first entry, key is ptr to the querynum */
	struct rbnode_t node;
	/** query id number, key for node */
	int querynum;
	/** was this an async query? */
	int async;

	/** for async query, the callback function */
	ub_val_callback_t cb;
	/** for async query, the callback user arg */
	void* cb_arg;

	/** result structure, also contains original query, type, class.
	 * malloced ptr ready to hand to the client. */
	struct ub_val_result* res;
};

/**
 * The error constants
 */
enum ub_ctx_err {
	/** no error */
	UB_NOERROR = 0,
	/** alloc failure */
	UB_NOMEM,
	/** socket operation */
	UB_SOCKET,
	/** syntax error */
	UB_SYNTAX,
	/** DNS service failed */
	UB_SERVFAIL,
	/** initialization failed (bad settings) */
	UB_INITFAIL
};

/** 
 * finalize a context.
 * @param ctx: context to finalize. creates shared data.
 * @return 0 if OK, or errcode.
 */
int context_finalize(struct ub_val_ctx* ctx);

/** compare two ctx_query elements */
int context_query_cmp(const void* a, const void* b);

/**
 * Create new query in context, add to querynum list.
 * @param ctx: context
 * @param name: query name
 * @param rrtype: type
 * @param rrclass: class
 * @param cb: callback for async, or NULL for sync.
 * @param cbarg: user arg for async queries.
 * @return new ctx_query or NULL for malloc failure.
 */
struct ctx_query* context_new(struct ub_val_ctx* ctx, char* name, int rrtype,
        int rrclass, ub_val_callback_t cb, void* cbarg);

#endif /* LIBUNBOUND_CONTEXT_H */