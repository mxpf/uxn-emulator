#include "constellation.h"

#include <string.h>

static uint16_t
device_short(const Uxn *uxn, uint8_t port)
{
	return (uint16_t)((uxn->devices[port] << 8) |
		uxn->devices[(uint8_t)(port + 1u)]);
}

static void
trace_event(Constellation *constellation, ConstellationTraceKind kind,
	ConstellationEndpointId endpoint, ConstellationEndpointId peer,
	const ConstellationMessage *message)
{
	ConstellationTraceEvent *event;

	if(constellation->trace_count >= CONSTELLATION_TRACE_CAPACITY) {
		constellation->trace_exhausted = true;
		if(constellation->fault_reason == CONSTELLATION_FAULT_NONE)
			constellation->fault_reason = CONSTELLATION_FAULT_TRACE_EXHAUSTED;
		return;
	}
	event = &constellation->trace[constellation->trace_count++];
	memset(event, 0, sizeof(*event));
	event->kind = kind;
	if(kind == CONSTELLATION_TRACE_FAULT)
		event->reason = constellation->fault_reason;
	event->endpoint = endpoint;
	event->peer = peer;
	if(message)
		event->message = *message;
}

static void
fault(Constellation *c, ConstellationEndpointId id, ConstellationFault reason)
{
	if(c->fault_reason != CONSTELLATION_FAULT_NONE)
		return;
	c->fault_reason = reason;
	c->endpoints[id].faulted = true;
	trace_event(c, CONSTELLATION_TRACE_FAULT, id, id, NULL);
}

static bool
queue_push(ConstellationQueue *queue, const ConstellationMessage *message)
{
	uint8_t tail;

	if(queue->count == CONSTELLATION_QUEUE_CAPACITY)
		return false;
	tail = (uint8_t)((queue->head + queue->count) %
		CONSTELLATION_QUEUE_CAPACITY);
	queue->messages[tail] = *message;
	queue->count++;
	return true;
}

static bool
queue_pop(ConstellationQueue *queue, ConstellationMessage *message)
{
	if(!queue->count)
		return false;
	*message = queue->messages[queue->head];
	queue->head = (uint8_t)((queue->head + 1u) %
		CONSTELLATION_QUEUE_CAPACITY);
	queue->count--;
	return true;
}

static uint8_t
port_read(Uxn *uxn, uint8_t port, void *context)
{
	(void)context;
	return uxn->devices[port];
}

static void
port_write(Uxn *uxn, uint8_t port, uint8_t value, void *context)
{
	ConstellationEndpoint *endpoint = context;
	Constellation *constellation = endpoint->constellation;
	ConstellationEndpointId peer = endpoint->id == CONSTELLATION_A ?
		CONSTELLATION_B : CONSTELLATION_A;
	ConstellationMessage message;
	ConstellationQueue *outgoing;
	uint16_t address;
	uint16_t i;

	if(constellation_has_fault(constellation) ||
		port != CONSTELLATION_PORT_SEND || value == 0)
		return;
	memset(&message, 0, sizeof(message));
	message.length = uxn->devices[CONSTELLATION_PORT_SEND_LENGTH];
	address = device_short(uxn, CONSTELLATION_PORT_SEND_ADDRESS);
	for(i = 0; i < message.length; i++)
		message.data[i] = uxn->ram[(uint16_t)(address + i)];
	outgoing = &constellation->queues[endpoint->id];
	if(queue_push(outgoing, &message)) {
		uxn->devices[CONSTELLATION_PORT_SEND] = 1;
		trace_event(constellation, CONSTELLATION_TRACE_SEND,
			endpoint->id, peer, &message);
	} else {
		endpoint->waiting_for_space = true;
		uxn->devices[CONSTELLATION_PORT_SEND] = 0;
		trace_event(constellation, CONSTELLATION_TRACE_SEND_FULL,
			endpoint->id, peer, &message);
	}
}

static bool
run_vector(Constellation *constellation, ConstellationEndpoint *endpoint,
	uint16_t vector)
{
	if(constellation_has_fault(constellation))
		return false;
	if(uxn_eval(&endpoint->uxn, vector, constellation->instruction_ceiling) ==
		UXN_STOP_BREAK)
		return !constellation_has_fault(constellation);
	fault(constellation, endpoint->id, CONSTELLATION_FAULT_INSTRUCTION_LIMIT);
	return false;
}

void
constellation_init(Constellation *constellation, uint64_t instruction_ceiling)
{
	ConstellationEndpointId id;

	memset(constellation, 0, sizeof(*constellation));
	constellation->instruction_ceiling = instruction_ceiling;
	constellation->next_turn = CONSTELLATION_B;
	for(id = CONSTELLATION_A; id < CONSTELLATION_ENDPOINTS; id++) {
		ConstellationEndpoint *endpoint = &constellation->endpoints[id];
		uxn_init(&endpoint->uxn);
		endpoint->constellation = constellation;
		endpoint->id = id;
		uxn_connect_devices(&endpoint->uxn, port_read, port_write, endpoint);
	}
	if(!instruction_ceiling) {
		fault(constellation, CONSTELLATION_A, CONSTELLATION_FAULT_INVALID_CEILING);
	}
}

bool
constellation_load(Constellation *constellation,
	ConstellationEndpointId endpoint, const uint8_t *rom, size_t length)
{
	if((unsigned)endpoint >= CONSTELLATION_ENDPOINTS ||
		constellation->booted || constellation_has_fault(constellation) ||
		length > UXN_RAM_SIZE - UXN_ROM_START)
		return false;
	if(constellation->endpoints[endpoint].loaded)
		return false;
	if(!uxn_load(&constellation->endpoints[endpoint].uxn, rom, length))
		return false;
	constellation->endpoints[endpoint].loaded = true;
	return true;
}

bool
constellation_boot(Constellation *constellation)
{
	ConstellationEndpointId order[] = {CONSTELLATION_B, CONSTELLATION_A};
	size_t i;

	if(constellation->booted || constellation_has_fault(constellation))
		return false;
	constellation->booted = true;
	/* Boot the receiver first so both port vectors exist before delivery. */
	for(i = 0; i < sizeof(order) / sizeof(order[0]); i++) {
		ConstellationEndpoint *endpoint =
			&constellation->endpoints[order[i]];
		trace_event(constellation, CONSTELLATION_TRACE_BOOT, order[i],
			order[i], NULL);
		if(!run_vector(constellation, endpoint, UXN_ROM_START))
			return false;
	}
	return true;
}

bool
constellation_step(Constellation *constellation)
{
	ConstellationEndpointId id = constellation->next_turn;
	ConstellationEndpointId sender = id == CONSTELLATION_A ?
		CONSTELLATION_B : CONSTELLATION_A;
	ConstellationEndpoint *endpoint = &constellation->endpoints[id];
	ConstellationQueue *incoming = &constellation->queues[sender];
	ConstellationMessage message;
	uint16_t address;
	uint16_t vector;
	uint16_t i;

	if(!constellation->booted || constellation_has_fault(constellation))
		return false;
	constellation->next_turn = sender;
	trace_event(constellation, CONSTELLATION_TRACE_TURN, id, id, NULL);
	if(constellation_has_fault(constellation))
		return false;
	/* Space notifications take priority and consume this endpoint's turn. */
	if(endpoint->waiting_for_space &&
		constellation->queues[id].count < CONSTELLATION_QUEUE_CAPACITY) {
		endpoint->waiting_for_space = false;
		vector = device_short(&endpoint->uxn,
			CONSTELLATION_PORT_WRITABLE_VECTOR);
		if(!vector) {
			fault(constellation, id, CONSTELLATION_FAULT_WRITABLE_VECTOR);
			return false;
		}
		trace_event(constellation, CONSTELLATION_TRACE_WRITABLE, id, id, NULL);
		return run_vector(constellation, endpoint, vector);
	}
	if(!queue_pop(incoming, &message)) {
		trace_event(constellation, CONSTELLATION_TRACE_IDLE, id, id, NULL);
		return !constellation_has_fault(constellation);
	}
	address = device_short(&endpoint->uxn,
		CONSTELLATION_PORT_RECEIVE_ADDRESS);
	for(i = 0; i < message.length; i++)
		endpoint->uxn.ram[(uint16_t)(address + i)] = message.data[i];
	endpoint->uxn.devices[CONSTELLATION_PORT_RECEIVE_LENGTH] = message.length;
	endpoint->uxn.devices[CONSTELLATION_PORT_RECEIVE_READY] = 1;
	trace_event(constellation, CONSTELLATION_TRACE_DELIVER, sender, id,
		&message);
	vector = device_short(&endpoint->uxn, CONSTELLATION_PORT_VECTOR);
	if(!vector) {
		endpoint->uxn.devices[CONSTELLATION_PORT_RECEIVE_READY] = 0;
		fault(constellation, id, CONSTELLATION_FAULT_RECEIVE_VECTOR);
		return false;
	}
	if(!run_vector(constellation, endpoint, vector)) {
		endpoint->uxn.devices[CONSTELLATION_PORT_RECEIVE_READY] = 0;
		return false;
	}
	endpoint->uxn.devices[CONSTELLATION_PORT_RECEIVE_READY] = 0;
	return true;
}

bool
constellation_is_quiescent(const Constellation *constellation)
{
	return constellation->queues[CONSTELLATION_A].count == 0 &&
		constellation->queues[CONSTELLATION_B].count == 0 &&
		!constellation->endpoints[0].waiting_for_space &&
		!constellation->endpoints[1].waiting_for_space;
}

bool
constellation_has_fault(const Constellation *constellation)
{
	return constellation->trace_exhausted ||
		constellation->endpoints[CONSTELLATION_A].faulted ||
		constellation->endpoints[CONSTELLATION_B].faulted;
}

const Uxn *
constellation_uxn(const Constellation *constellation,
	ConstellationEndpointId endpoint)
{
	if((unsigned)endpoint >= CONSTELLATION_ENDPOINTS)
		return NULL;
	return &constellation->endpoints[endpoint].uxn;
}

size_t
constellation_trace_count(const Constellation *constellation)
{
	return constellation->trace_count;
}

const ConstellationTraceEvent *
constellation_trace_event(const Constellation *constellation, size_t index)
{
	if(index >= constellation->trace_count)
		return NULL;
	return &constellation->trace[index];
}

const char *
constellation_endpoint_name(ConstellationEndpointId endpoint)
{
	return endpoint == CONSTELLATION_A ? "A" : "B";
}

const char *
constellation_fault_name(ConstellationFault reason)
{
	switch(reason) {
	case CONSTELLATION_FAULT_NONE: return "none";
	case CONSTELLATION_FAULT_INVALID_CEILING: return "invalid-instruction-ceiling";
	case CONSTELLATION_FAULT_INSTRUCTION_LIMIT: return "instruction-limit";
	case CONSTELLATION_FAULT_RECEIVE_VECTOR: return "missing-receive-vector";
	case CONSTELLATION_FAULT_WRITABLE_VECTOR: return "missing-writable-vector";
	case CONSTELLATION_FAULT_TRACE_EXHAUSTED: return "trace-exhausted";
	}
	return "unknown";
}

const char *
constellation_trace_kind_name(ConstellationTraceKind kind)
{
	switch(kind) {
	case CONSTELLATION_TRACE_BOOT: return "boot";
	case CONSTELLATION_TRACE_TURN: return "turn";
	case CONSTELLATION_TRACE_SEND: return "send";
	case CONSTELLATION_TRACE_SEND_FULL: return "send-full";
	case CONSTELLATION_TRACE_DELIVER: return "deliver";
	case CONSTELLATION_TRACE_IDLE: return "idle";
	case CONSTELLATION_TRACE_WRITABLE: return "writable";
	case CONSTELLATION_TRACE_FAULT: return "fault";
	}
	return "unknown";
}
