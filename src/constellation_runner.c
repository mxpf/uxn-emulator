#include "constellation_runner.h"
#include <stdlib.h>
#include <string.h>

typedef struct RunnerBinding {
	ConstellationRunner *runner;
	unsigned node;
	UxnDeviceRead read;
	UxnDeviceWrite write;
	void *context;
} RunnerBinding;

static uint8_t
read_device(Uxn *u, uint8_t port, void *context)
{
	RunnerBinding *b = context;
	for(size_t i = 0; i < b->runner->device_count; i++) {
		const RunnerDevice *d = &b->runner->devices[i];
		if(d->node == b->node && port >= d->first && port <= d->last && d->read)
			return d->read(u, port, d->context);
	}
	return b->read(u, port, b->context);
}

static void
write_device(Uxn *u, uint8_t port, uint8_t value, void *context)
{
	RunnerBinding *b = context;
	for(size_t i = 0; i < b->runner->device_count; i++) {
		const RunnerDevice *d = &b->runner->devices[i];
		if(d->node == b->node && port >= d->first && port <= d->last && d->write) {
			d->write(u, port, value, d->context); return;
		}
	}
	b->write(u, port, value, b->context);
}

void
runner_free(ConstellationRunner *r)
{
	if(!r) return;
	free(r->host.nodes); free(r->host.links); free(r->devices); free(r->bindings);
	memset(r, 0, sizeof(*r));
}

static bool
reject(RunnerDiagnostic *out, RunnerStage stage, RunnerReason reason, unsigned node, RoutedFault fault)
{
	if(out) *out = (RunnerDiagnostic){stage, reason, node, fault};
	return false;
}

const char *
runner_diagnostic_text(const RunnerDiagnostic *d)
{
	if(!d) return "No startup diagnostic available.";
	switch(d->reason) {
	case RUNNER_SUCCESS: return "Ready.";
	case RUNNER_INVALID_ARGUMENT: return "Missing runner or application definition.";
	case RUNNER_ALREADY_LIVE: return "Runner is already active.";
	case RUNNER_BAD_CONFIG: return "Invalid application counts, storage or instruction ceiling.";
	case RUNNER_BAD_ROM: return "Missing, oversized or rejected ROM bytes.";
	case RUNNER_BAD_DEVICE: return "Invalid device attachment or reserved port range.";
	case RUNNER_DEVICE_OVERLAP: return "Device attachments overlap.";
	case RUNNER_BAD_ROUTES: return "Invalid or duplicate route declaration.";
	case RUNNER_NO_MEMORY: return "Not enough memory to start the application.";
	case RUNNER_HOST_FAULT:
		switch(d->fault) {
		case ROUTED_LIMIT: return "ROM exceeded its boot instruction limit.";
		case ROUTED_BAD_ROUTE: return "ROM sent to an undeclared route during boot.";
		case ROUTED_TRACE_FULL: return "Boot exceeded the bounded trace capacity.";
		default: return "Routed host rejected startup.";
		}
	}
	return "Unknown startup failure.";
}

bool
runner_start(ConstellationRunner *r, const RunnerDefinition *d, RunnerDiagnostic *out)
{
	if(out) *out = (RunnerDiagnostic){RUNNER_STAGE_NONE, RUNNER_SUCCESS, ROUTED_NONE, ROUTED_OK};
	if(!r || !d) return reject(out, RUNNER_VALIDATE, RUNNER_INVALID_ARGUMENT, ROUTED_NONE, ROUTED_OK);
	if(r->host.nodes || r->host.links || r->devices || r->bindings)
		return reject(out, RUNNER_VALIDATE, RUNNER_ALREADY_LIVE, ROUTED_NONE, ROUTED_OK);
	if(!d->roms || !d->node_count || d->node_count > ROUTED_MAX_NODES ||
		d->route_count > ROUTED_MAX_ROUTES || (d->route_count && !d->routes) || !d->ceiling ||
		(d->device_count && !d->devices) || d->device_count > d->node_count * UXN_DEVICE_SIZE)
		return reject(out, RUNNER_VALIDATE, RUNNER_BAD_CONFIG, ROUTED_NONE, ROUTED_OK);
	for(size_t i = 0; i < d->node_count; i++)
		if(!d->roms[i].bytes || d->roms[i].length > UXN_RAM_SIZE - UXN_ROM_START)
			return reject(out, RUNNER_VALIDATE, RUNNER_BAD_ROM, (unsigned)i, ROUTED_OK);
	uint8_t occupied[ROUTED_MAX_NODES][UXN_DEVICE_SIZE / 8] = {{0}};
	for(size_t i = 0; i < d->device_count; i++) {
		const RunnerDevice *a = &d->devices[i];
		if(a->node >= d->node_count || a->first > a->last || a->last >= UXN_DEVICE_SIZE ||
			(a->first <= 0xdf && a->last >= 0xd0) || (!a->read && !a->write))
			return reject(out, RUNNER_VALIDATE, RUNNER_BAD_DEVICE, a->node < d->node_count ? a->node : ROUTED_NONE, ROUTED_OK);
		for(unsigned port = a->first; port <= a->last; port++) {
			uint8_t mask = (uint8_t)(1u << (port % 8));
			if(occupied[a->node][port / 8] & mask)
				return reject(out, RUNNER_VALIDATE, RUNNER_DEVICE_OVERLAP, a->node, ROUTED_OK);
			occupied[a->node][port / 8] |= mask;
		}
	}
	RoutedNode *nodes = calloc(d->node_count, sizeof(*nodes));
	RoutedLink *links = d->route_count ? calloc(d->route_count, sizeof(*links)) : NULL;
	if(!nodes || (d->route_count && !links)) {
		free(nodes); free(links); return reject(out, RUNNER_ALLOCATE, RUNNER_NO_MEMORY, ROUTED_NONE, ROUTED_OK);
	}
	if(!routed_init(&r->host, nodes, d->node_count, links, d->routes, d->route_count, d->ceiling)) {
		RoutedFault fault = r->host.fault;
		free(nodes); free(links); memset(r, 0, sizeof(*r));
		return reject(out, RUNNER_VALIDATE, RUNNER_BAD_ROUTES, ROUTED_NONE, fault);
	}
	for(size_t i = 0; i < d->node_count; i++)
		if(!routed_load(&r->host, (unsigned)i, d->roms[i].bytes, d->roms[i].length)) {
			reject(out, RUNNER_LOAD, RUNNER_BAD_ROM, (unsigned)i, r->host.fault); goto bad;
		}
	if(d->device_count) {
		r->devices = malloc(d->device_count * sizeof(*r->devices));
		r->bindings = calloc(d->node_count, sizeof(*r->bindings));
		if(!r->devices || !r->bindings) {
			reject(out, RUNNER_ALLOCATE, RUNNER_NO_MEMORY, ROUTED_NONE, ROUTED_OK); goto bad;
		}
		memcpy(r->devices, d->devices, d->device_count * sizeof(*r->devices));
		r->device_count = d->device_count;
		for(size_t i = 0; i < d->node_count; i++) {
			Uxn *u = &nodes[i].uxn;
			r->bindings[i] = (RunnerBinding){r, (unsigned)i, u->device_read, u->device_write, u->device_context};
			uxn_connect_devices(u, read_device, write_device, &r->bindings[i]);
		}
	}
	if(!routed_boot(&r->host)) {
		unsigned node = ROUTED_NONE;
		for(size_t i = 0; i < r->host.trace_count; i++)
			if(r->host.trace[i].kind == CONSTELLATION_TRACE_FAULT) { node = r->host.trace[i].from; break; }
		reject(out, RUNNER_BOOT, RUNNER_HOST_FAULT, node, r->host.fault); goto bad;
	}
	return true;
bad:
	runner_free(r); return false;
}
