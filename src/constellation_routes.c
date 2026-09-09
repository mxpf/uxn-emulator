#include "constellation_routes.h"
#include <string.h>

static uint16_t
short_at(const Uxn *u, uint8_t port)
{
	return (uint16_t)((u->devices[port] << 8) | u->devices[port + 1]);
}

static void
event(RoutedHost *h, ConstellationTraceKind kind, uint8_t from, uint8_t to,
	uint8_t selector, const ConstellationMessage *message)
{
	if(h->trace_count == CONSTELLATION_TRACE_CAPACITY || h->trace_sequence == UINT64_MAX) {
		h->trace_exhausted = true;
		if(h->fault == ROUTED_OK) h->fault = ROUTED_TRACE_FULL;
		return;
	}
	RoutedEvent *e = &h->trace[h->trace_count++];
	memset(e, 0, sizeof(*e));
	e->sequence = h->trace_sequence++;
	e->kind = kind; e->from = from; e->to = to; e->selector = selector;
	if(kind == CONSTELLATION_TRACE_FAULT) e->reason = h->fault;
	if(message) e->message = *message;
}

static void
fail(RoutedHost *h, uint8_t node, RoutedFault reason, uint8_t selector)
{
	if(h->fault != ROUTED_OK) return;
	h->fault = reason;
	event(h, CONSTELLATION_TRACE_FAULT, node,
		reason == ROUTED_BAD_ROUTE ? ROUTED_NONE : node, selector, NULL);
}

static uint8_t
read_port(Uxn *u, uint8_t port, void *context)
{
	const RoutedNode *n = context;
	if(port == ROUTED_RECEIVE_SOURCE) return n->source;
	if(port == ROUTED_WRITABLE_ROUTE) return n->writable;
	return u->devices[port];
}

static void
write_port(Uxn *u, uint8_t port, uint8_t value, void *context)
{
	RoutedNode *n = context;
	RoutedHost *h = n->host;
	RoutedLink *link = NULL;
	ConstellationMessage message;
	if(h->fault != ROUTED_OK || port != CONSTELLATION_PORT_SEND || !value) return;
	uint8_t selector = u->devices[ROUTED_SEND_ROUTE];
	for(size_t i = 0; i < h->route_count; i++)
		if(h->links[i].route.from == n->id && h->links[i].route.selector == selector) {
			link = &h->links[i]; break;
		}
	if(!link) {
		u->devices[CONSTELLATION_PORT_SEND] = 0;
		fail(h, n->id, ROUTED_BAD_ROUTE, selector);
		return;
	}
	memset(&message, 0, sizeof(message));
	message.length = u->devices[CONSTELLATION_PORT_SEND_LENGTH];
	uint16_t address = short_at(u, CONSTELLATION_PORT_SEND_ADDRESS);
	for(unsigned i = 0; i < message.length; i++) message.data[i] = u->ram[(uint16_t)(address + i)];
	ConstellationQueue *q = &link->queue;
	if(q->count == CONSTELLATION_QUEUE_CAPACITY) {
		link->waiting = true;
		u->devices[CONSTELLATION_PORT_SEND] = 0;
		event(h, CONSTELLATION_TRACE_SEND_FULL, n->id, link->route.to, selector, &message);
	} else {
		q->messages[(q->head + q->count) % CONSTELLATION_QUEUE_CAPACITY] = message;
		q->count++;
		u->devices[CONSTELLATION_PORT_SEND] = 1;
		event(h, CONSTELLATION_TRACE_SEND, n->id, link->route.to, selector, &message);
	}
}

static bool
run(RoutedHost *h, RoutedNode *n, uint16_t vector)
{
	if(h->fault != ROUTED_OK) return false;
	if(uxn_eval(&n->uxn, vector, h->ceiling) != UXN_STOP_BREAK) fail(h, n->id, ROUTED_LIMIT, ROUTED_NONE);
	return h->fault == ROUTED_OK;
}

bool
routed_init(RoutedHost *h, RoutedNode *nodes, size_t count,
	RoutedLink *links, const RoutedRoute *routes, size_t route_count, uint64_t ceiling)
{
	memset(h, 0, sizeof(*h));
	if(!nodes || !count || count > ROUTED_MAX_NODES || route_count > ROUTED_MAX_ROUTES ||
		(route_count && (!links || !routes)) || !ceiling) {
		h->fault = ROUTED_BAD_CONFIG; return false;
	}
	for(size_t i = 0; i < route_count; i++) {
		if(routes[i].from >= count || routes[i].to >= count || routes[i].selector == ROUTED_NONE) {
			h->fault = ROUTED_BAD_CONFIG; return false;
		}
		for(size_t j = 0; j < i; j++)
			if(routes[i].from == routes[j].from && routes[i].selector == routes[j].selector) {
				h->fault = ROUTED_BAD_CONFIG; return false;
			}
	}
	h->nodes = nodes; h->node_count = count; h->links = links; h->route_count = route_count; h->ceiling = ceiling;
	memset(nodes, 0, count * sizeof(*nodes));
	for(size_t i = 0; i < count; i++) {
		RoutedNode *n = &nodes[i];
		uxn_init(&n->uxn); n->host = h; n->id = (uint8_t)i;
		n->source = n->writable = ROUTED_NONE;
		uxn_connect_devices(&n->uxn, read_port, write_port, n);
	}
	for(size_t i = 0; i < route_count; i++) {
		memset(&links[i], 0, sizeof(links[i])); links[i].route = routes[i];
	}
	return true;
}

bool
routed_load(RoutedHost *h, unsigned node, const uint8_t *rom, size_t length)
{
	if(h->fault != ROUTED_OK || h->booted || node >= h->node_count ||
		h->nodes[node].loaded || !rom || length > UXN_RAM_SIZE - UXN_ROM_START) return false;
	if(!uxn_load(&h->nodes[node].uxn, rom, length)) return false;
	h->nodes[node].loaded = true;
	return true;
}

bool
routed_boot(RoutedHost *h)
{
	if(h->fault != ROUTED_OK || h->booted || !h->node_count) return false;
	for(size_t i = 0; i < h->node_count; i++) if(!h->nodes[i].loaded) return false;
	h->booted = true;
	/* Every boot completes before any queued payload can be delivered. */
	for(size_t i = 0; i < h->node_count; i++) {
		event(h, CONSTELLATION_TRACE_BOOT, (uint8_t)i, (uint8_t)i, ROUTED_NONE, NULL);
		if(!run(h, &h->nodes[i], UXN_ROM_START)) return false;
	}
	return true;
}

bool
routed_step(RoutedHost *h)
{
	if(!h->booted || h->fault != ROUTED_OK) return false;
	RoutedNode *n = &h->nodes[h->next_node];
	h->next_node = (h->next_node + 1) % h->node_count;
	event(h, CONSTELLATION_TRACE_TURN, n->id, n->id, ROUTED_NONE, NULL);
	if(h->fault != ROUTED_OK) return false;
	size_t writable = h->route_count, incoming = h->route_count;
	/* Independent pending notifications; changing dc cannot redirect a wake-up. */
	for(size_t offset = 0; offset < h->route_count; offset++) {
		size_t i = (n->next_writable + offset) % h->route_count;
		RoutedLink *l = &h->links[i];
		if(l->route.from != n->id || !l->waiting || l->queue.count == CONSTELLATION_QUEUE_CAPACITY) continue;
		writable = i; break;
	}
	for(size_t offset = 0; offset < h->route_count; offset++) {
		size_t i = (n->next_incoming + offset) % h->route_count;
		RoutedLink *l = &h->links[i];
		if(l->route.to != n->id || !l->queue.count) continue;
		incoming = i; break;
	}
	/* Alternate ready classes; looking ahead must not advance either cursor. */
	if(writable < h->route_count && (incoming == h->route_count || !n->prefer_receive)) {
		size_t i = writable;
		RoutedLink *l = &h->links[i];
		n->prefer_receive = true;
		l->waiting = false; n->next_writable = (i + 1) % h->route_count;
		n->writable = l->route.selector;
		event(h, CONSTELLATION_TRACE_WRITABLE, n->id, l->route.to, l->route.selector, NULL);
		uint16_t vector = short_at(&n->uxn, CONSTELLATION_PORT_WRITABLE_VECTOR);
		if(!vector) fail(h, n->id, ROUTED_WRITABLE_VECTOR_MISSING, ROUTED_NONE);
		bool ok = run(h, n, vector);
		n->writable = ROUTED_NONE;
		return ok;
	}
	if(incoming < h->route_count) {
		size_t i = incoming;
		RoutedLink *l = &h->links[i];
		n->prefer_receive = false;
		ConstellationMessage message = l->queue.messages[l->queue.head];
		l->queue.head = (l->queue.head + 1) % CONSTELLATION_QUEUE_CAPACITY; l->queue.count--;
		n->next_incoming = (i + 1) % h->route_count;
		uint16_t address = short_at(&n->uxn, CONSTELLATION_PORT_RECEIVE_ADDRESS);
		for(unsigned j = 0; j < message.length; j++) n->uxn.ram[(uint16_t)(address + j)] = message.data[j];
		n->uxn.devices[CONSTELLATION_PORT_RECEIVE_LENGTH] = message.length;
		n->uxn.devices[CONSTELLATION_PORT_RECEIVE_READY] = 1;
		n->source = l->route.from;
		event(h, CONSTELLATION_TRACE_DELIVER, l->route.from, n->id, l->route.selector, &message);
		uint16_t vector = short_at(&n->uxn, CONSTELLATION_PORT_VECTOR);
		if(!vector) fail(h, n->id, ROUTED_RECEIVE_VECTOR_MISSING, ROUTED_NONE);
		bool ok = run(h, n, vector);
		n->uxn.devices[CONSTELLATION_PORT_RECEIVE_READY] = 0; n->source = ROUTED_NONE;
		return ok;
	}
	event(h, CONSTELLATION_TRACE_IDLE, n->id, n->id, ROUTED_NONE, NULL);
	return h->fault == ROUTED_OK;
}

bool
routed_quiescent(const RoutedHost *h)
{
	for(size_t i = 0; i < h->route_count; i++)
		if(h->links[i].queue.count || h->links[i].waiting) return false;
	return true;
}

bool
routed_take_trace(RoutedHost *h, RoutedEvent *output, size_t capacity, size_t *count)
{
	if(!count || capacity < h->trace_count || (h->trace_count && !output)) return false;
	*count = h->trace_count;
	if(h->trace_count) memcpy(output, h->trace, h->trace_count * sizeof(*output));
	memset(h->trace, 0, sizeof(h->trace));
	h->trace_count = 0;
	return true;
}
