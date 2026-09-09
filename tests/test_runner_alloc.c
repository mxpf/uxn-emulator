#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned calls, fail_at, live, checks;
#define CHECK(x) do { checks++; if(!(x)) { fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #x); exit(1); } } while(0)
static void *tracked_malloc(size_t size)
{
	if(calls++ == fail_at) return NULL;
	void *p = malloc(size); if(p) live++; return p;
}
static void *tracked_calloc(size_t count, size_t size)
{
	if(calls++ == fail_at) return NULL;
	void *p = calloc(count, size); if(p) live++; return p;
}
static void tracked_free(void *p) { if(p) { CHECK(live > 0); live--; } free(p); }
/* Fault injection is local to this translation unit, not a production API. */
#define malloc tracked_malloc
#define calloc tracked_calloc
#define free tracked_free
#include "../src/constellation_runner.c"
#undef malloc
#undef calloc
#undef free

static void write_device_stub(Uxn *u, uint8_t port, uint8_t value, void *context)
{ (void)u; (void)port; (void)value; (void)context; }

int main(void)
{
	const uint8_t idle[] = {0}; const RunnerRom rom = {idle,1};
	const RoutedRoute route = {0,1,0};
	const RunnerDevice device = {0,0x20,0x20,NULL,write_device_stub,NULL};
	const RunnerDefinition d = {&rom,1,&route,1,&device,1,100};
	for(fail_at = 0; fail_at <= 4; fail_at++) {
		ConstellationRunner r = {0}, empty = {0}; RunnerDiagnostic error; calls = 0;
		bool ok = runner_start(&r, &d, &error);
		CHECK(ok == (fail_at == 4));
		if(!ok) {
			CHECK(error.stage == RUNNER_ALLOCATE && error.reason == RUNNER_NO_MEMORY && error.node == ROUTED_NONE);
			CHECK(memcmp(&r, &empty, sizeof(r)) == 0); CHECK(live == 0);
		} else CHECK(error.reason == RUNNER_SUCCESS && live == 4);
		runner_free(&r); CHECK(live == 0);
		if(!ok) CHECK(error.reason == RUNNER_NO_MEMORY);
	}
	printf("%u runner allocation checks passed.\n", checks); return 0;
}
