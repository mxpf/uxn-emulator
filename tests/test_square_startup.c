#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned checks, opens;
static int missing = -1, looping = -1;
#define CHECK(x) do { checks++; if(!(x)) { fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #x); exit(1); } } while(0)
static FILE *fixture_open(const char *path, const char *mode)
{
	(void)path; (void)mode; int node = (int)(opens++ % 3);
	if(node == missing) return NULL;
	FILE *file = tmpfile(); CHECK(file);
	const unsigned char idle[] = {0}, loop[] = {0x40,0xff,0xfd};
	const unsigned char *bytes = node == looping ? loop : idle;
	size_t length = node == looping ? sizeof(loop) : sizeof(idle);
	CHECK(fwrite(bytes, 1, length, file) == length); rewind(file); return file;
}
#define fopen fixture_open
#include "../src/square_demo.c"
#undef fopen

int main(void)
{
	SquareSession *s = calloc(1, sizeof(*s)); CHECK(s);
	missing = 1;
	CHECK(!square_init(s)); CHECK(s->failed && !s->live);
	CHECK(strstr(s->error, "Cannot open build/routes-square-world.rom")); square_free(s);
	missing = -1; looping = 1; opens = 0;
	CHECK(!square_init(s)); CHECK(s->failed && !s->live);
	CHECK(strstr(s->error, "Node 1: ROM exceeded its boot instruction limit.")); square_free(s);
	looping = -1; opens = 0; CHECK(square_init(s));
	looping = 1; CHECK(!square_replay_begin(s)); CHECK(s->failed && s->live && !s->play);
	CHECK(strstr(s->error, "Node 1: ROM exceeded its boot instruction limit."));
	square_free(s); free(s);
	printf("%u square startup checks passed.\n", checks); return 0;
}
