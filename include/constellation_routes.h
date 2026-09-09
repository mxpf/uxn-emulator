#ifndef CONSTELLATION_ROUTES_H
#define CONSTELLATION_ROUTES_H

/* Experimental routed host. Reuses v0's bounded message storage, not its ABI. */
#include "constellation.h"

enum { ROUTED_MAX_NODES = 255, ROUTED_MAX_ROUTES = 255, ROUTED_NONE = 255,
	ROUTED_SEND_ROUTE = 0xdc, ROUTED_RECEIVE_SOURCE = 0xdd,
	ROUTED_WRITABLE_ROUTE = 0xde };

typedef struct { uint8_t from, selector, to; } RoutedRoute;
/* ROUTED_NONE as a route source declares an external input, never a node. */
typedef enum {
	ROUTED_INPUT_ACCEPTED, ROUTED_INPUT_FULL, ROUTED_INPUT_INVALID,
	ROUTED_INPUT_FAULT
} RoutedInputResult;
typedef enum {
	ROUTED_OK, ROUTED_BAD_CONFIG, ROUTED_BAD_ROUTE, ROUTED_LIMIT,
	ROUTED_RECEIVE_VECTOR_MISSING, ROUTED_WRITABLE_VECTOR_MISSING,
	ROUTED_TRACE_FULL
} RoutedFault;
typedef struct {
	uint64_t sequence;
	ConstellationTraceKind kind;
	RoutedFault reason;
	uint8_t from, to, selector;
	ConstellationMessage message;
} RoutedEvent;

struct RoutedHost;
typedef struct {
	Uxn uxn;
	struct RoutedHost *host;
	size_t next_incoming, next_writable;
	uint8_t id, source, writable;
	bool loaded;
	bool prefer_receive; /* Prefer the opposite of the last serviced callback. */
} RoutedNode;
typedef struct {
	RoutedRoute route;
	ConstellationQueue queue;
	bool waiting;
} RoutedLink;
typedef struct RoutedHost {
	RoutedNode *nodes;
	RoutedLink *links;
	size_t node_count, route_count, next_node, trace_count;
	uint64_t trace_sequence; /* Next event number; unaffected by taking batches. */
	uint64_t ceiling;
	bool booted;
	bool trace_exhausted; /* An event was omitted, independently of first fault. */
	RoutedFault fault;
	RoutedEvent trace[CONSTELLATION_TRACE_CAPACITY];
} RoutedHost;

/* Caller owns disjoint node/link storage for the lifetime of the host.
 * Route definitions are copied; counts/topology cannot change after init. */
bool routed_init(RoutedHost *h, RoutedNode *nodes, size_t node_count,
	RoutedLink *links, const RoutedRoute *routes, size_t route_count,
	uint64_t ceiling);
bool routed_load(RoutedHost *h, unsigned node, const uint8_t *rom, size_t length);
bool routed_boot(RoutedHost *h);
bool routed_step(RoutedHost *h);
/* Submit only between evaluations, after boot, on a declared external selector.
 * Copies 0..255 bytes; never invokes guest code. FULL requires caller-owned
 * retention/retry. Record call order and completed-turn boundary for replay.
 * INVALID leaves all state unchanged; FAULT is terminal. */
RoutedInputResult routed_input(RoutedHost *h, unsigned selector,
	const uint8_t *data, size_t length);
bool routed_quiescent(const RoutedHost *h);
/* Between boot/step calls, copy the entire pending batch to disjoint caller
 * storage, then clear only that buffer. Failure leaves host/output unchanged.
 * Faults remain terminal; taking a truncated trace cannot repair it. */
bool routed_take_trace(RoutedHost *h, RoutedEvent *output, size_t capacity,
	size_t *count);

#endif
