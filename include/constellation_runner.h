#ifndef CONSTELLATION_RUNNER_H
#define CONSTELLATION_RUNNER_H
#include "constellation_routes.h"

/* Trusted host declarations, not a file format or guest-visible roles. */
typedef struct { const uint8_t *bytes; size_t length; } RunnerRom;
typedef struct {
	unsigned node, first, last; /* Inclusive device-port range; Port d0-df reserved. */
	UxnDeviceRead read;
	UxnDeviceWrite write;
	void *context;
} RunnerDevice;
typedef struct {
	const RunnerRom *roms; size_t node_count;
	const RoutedRoute *routes; size_t route_count;
	const RunnerDevice *devices; size_t device_count;
	uint64_t ceiling;
} RunnerDefinition;
typedef enum { RUNNER_STAGE_NONE, RUNNER_VALIDATE, RUNNER_ALLOCATE, RUNNER_LOAD, RUNNER_BOOT } RunnerStage;
typedef enum {
	RUNNER_SUCCESS, RUNNER_INVALID_ARGUMENT, RUNNER_ALREADY_LIVE, RUNNER_BAD_CONFIG,
	RUNNER_BAD_ROM, RUNNER_BAD_DEVICE, RUNNER_DEVICE_OVERLAP, RUNNER_BAD_ROUTES,
	RUNNER_NO_MEMORY, RUNNER_HOST_FAULT
} RunnerReason;
typedef struct {
	RunnerStage stage;
	RunnerReason reason;
	unsigned node; /* ROUTED_NONE when no valid node can be identified. */
	RoutedFault fault;
} RunnerDiagnostic;
struct RunnerBinding;
typedef struct {
	RoutedHost host;
	RunnerDevice *devices;
	struct RunnerBinding *bindings;
	size_t device_count;
} ConstellationRunner;

/* Start only zero-initialized/freed storage; a live runner cannot be restarted.
 * Copies ROM bytes/routes/attachments. Device contexts remain caller-owned and
 * must outlive the runner. Neither runner nor contexts may move while live.
 * Rejects invalid definitions before guest execution. Failed boot is not a
 * rollback of external device effects; all runner-owned storage is released.
 * Optional diagnostic output must be disjoint from runner/definition storage.
 * It is reset on every call, survives cleanup, and reports refused live starts
 * without changing the live runner. Uses routed_step/input/take_trace after boot. */
bool runner_start(ConstellationRunner *runner, const RunnerDefinition *definition,
	RunnerDiagnostic *diagnostic);
const char *runner_diagnostic_text(const RunnerDiagnostic *diagnostic);
void runner_free(ConstellationRunner *runner);
#endif
