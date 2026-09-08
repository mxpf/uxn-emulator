#ifndef CONSTELLATION_H
#define CONSTELLATION_H

#include "uxn.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum {
	CONSTELLATION_ENDPOINTS = 2,
	CONSTELLATION_QUEUE_CAPACITY = 4,
	CONSTELLATION_MESSAGE_MAX = 255,
	CONSTELLATION_TRACE_CAPACITY = 128,
	CONSTELLATION_PORT_VECTOR = 0xd0,
	CONSTELLATION_PORT_RECEIVE_ADDRESS = 0xd2,
	CONSTELLATION_PORT_RECEIVE_LENGTH = 0xd4,
	CONSTELLATION_PORT_RECEIVE_READY = 0xd5,
	CONSTELLATION_PORT_SEND_ADDRESS = 0xd6,
	CONSTELLATION_PORT_SEND_LENGTH = 0xd8,
	CONSTELLATION_PORT_SEND = 0xd9,
	CONSTELLATION_PORT_WRITABLE_VECTOR = 0xda
};

typedef enum {
	CONSTELLATION_A,
	CONSTELLATION_B
} ConstellationEndpointId;

typedef enum {
	CONSTELLATION_TRACE_BOOT,
	CONSTELLATION_TRACE_TURN,
	CONSTELLATION_TRACE_SEND,
	CONSTELLATION_TRACE_SEND_FULL,
	CONSTELLATION_TRACE_DELIVER,
	CONSTELLATION_TRACE_IDLE,
	CONSTELLATION_TRACE_WRITABLE,
	CONSTELLATION_TRACE_FAULT
} ConstellationTraceKind;

typedef enum {
	CONSTELLATION_FAULT_NONE,
	CONSTELLATION_FAULT_INVALID_CEILING,
	CONSTELLATION_FAULT_INSTRUCTION_LIMIT,
	CONSTELLATION_FAULT_RECEIVE_VECTOR,
	CONSTELLATION_FAULT_WRITABLE_VECTOR,
	CONSTELLATION_FAULT_TRACE_EXHAUSTED
} ConstellationFault;

typedef struct {
	uint8_t length;
	uint8_t data[CONSTELLATION_MESSAGE_MAX];
} ConstellationMessage;

typedef struct {
	ConstellationMessage messages[CONSTELLATION_QUEUE_CAPACITY];
	uint8_t head;
	uint8_t count;
} ConstellationQueue;

typedef struct {
	ConstellationTraceKind kind;
	ConstellationFault reason;
	ConstellationEndpointId endpoint;
	ConstellationEndpointId peer;
	ConstellationMessage message;
} ConstellationTraceEvent;

struct Constellation;

typedef struct {
	Uxn uxn;
	struct Constellation *constellation;
	ConstellationEndpointId id;
	bool faulted;
	bool loaded;
	bool waiting_for_space;
} ConstellationEndpoint;

typedef struct Constellation {
	ConstellationEndpoint endpoints[CONSTELLATION_ENDPOINTS];
	ConstellationQueue queues[CONSTELLATION_ENDPOINTS];
	ConstellationTraceEvent trace[CONSTELLATION_TRACE_CAPACITY];
	size_t trace_count;
	ConstellationEndpointId next_turn;
	uint64_t instruction_ceiling;
	bool trace_exhausted;
	bool booted;
	ConstellationFault fault_reason;
} Constellation;

void constellation_init(Constellation *constellation,
	uint64_t instruction_ceiling);
bool constellation_load(Constellation *constellation,
	ConstellationEndpointId endpoint, const uint8_t *rom, size_t length);
bool constellation_boot(Constellation *constellation);
bool constellation_step(Constellation *constellation);
bool constellation_is_quiescent(const Constellation *constellation);
bool constellation_has_fault(const Constellation *constellation);

const Uxn *constellation_uxn(const Constellation *constellation,
	ConstellationEndpointId endpoint);
size_t constellation_trace_count(const Constellation *constellation);
const ConstellationTraceEvent *constellation_trace_event(
	const Constellation *constellation, size_t index);
const char *constellation_endpoint_name(ConstellationEndpointId endpoint);
const char *constellation_trace_kind_name(ConstellationTraceKind kind);
const char *constellation_fault_name(ConstellationFault reason);

#endif
