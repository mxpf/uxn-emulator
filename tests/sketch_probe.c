/* Test-only exports: not linked into the interactive browser prototype. */
#include "../src/sketch_web.c"
#include "sketch_fingerprint.h"
EXPORT int sketch_test_op(int op)
{
	if(op == '0') return sketch_reset();
	if(op >= '1' && op <= '6') return (int)sketch_input(&sketch, (unsigned)(op - '0'));
	if(op >= 'A' && op <= 'F') return sketch_action((unsigned)(op - 'A' + 1));
	if(op == '.') return sketch_step(&sketch);
	if(op == 'x') return (int)sketch_input(&sketch, 256);
	if(op == 'S') return sketch_export();
	if(op == 'L') return sketch_import(SKETCH_FILE_SIZE);
	if(op == '!') return sketch_import(SKETCH_FILE_SIZE - 1);
	if(op == '?') { document[16] = 2; int result = sketch_import(SKETCH_FILE_SIZE); document[16] = 0; return result; }
	return -1;
}
EXPORT const char *sketch_test_digest(void) { return fingerprint(&sketch); }
#ifndef __EMSCRIPTEN__
int main(void)
{
	int op;
	while((op = getchar()) != EOF) { int result = sketch_test_op(op); printf("%d %s\n", result, fingerprint(&sketch)); }
	sketch_free(&sketch); return 0;
}
#endif
