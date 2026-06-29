#ifndef RICH4_API_CONTRACT_H
#define RICH4_API_CONTRACT_H

/* Public API contract, version 1:
 * - const pointer inputs are borrowed for the duration of the call.
 * - mutable in/out pointers are caller-owned storage and are never retained,
 *   unless a declaration explicitly says BORROWED_LIFETIME.
 * - out_* pointers are required unless marked OPTIONAL; fallible functions
 *   initialize required outputs to a safe empty value before doing work.
 * - OWNED results must be released by the named matching free/destroy/close.
 * - release operations documented NULL_SAFE accept NULL.
 * See docs/api-ownership.md for the complete convention and exceptions. */
#define RICH4_API_CONTRACT_VERSION 1
#define RICH4_BORROWED
#define RICH4_BORROWED_LIFETIME
#define RICH4_CALLER_STORAGE
#define RICH4_OWNED
#define RICH4_OPTIONAL_OUT
#define RICH4_NULL_SAFE

#endif
