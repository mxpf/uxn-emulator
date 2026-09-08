#include <stdio.h>
int garden_web_reset(void);
int garden_web_step(int action);
const char *garden_web_digest(void);
int main(void)
{
	unsigned seed = 42;
	const int greeting[] = {2,2,2,2,2,3,3,3,3,5};
	if(!garden_web_reset()) return 1;
	puts(garden_web_digest());
	for(unsigned i = 0; i < 1010; i++) {
		seed = seed * 1664525u + 1013904223u;
		int action = i < 10 ? greeting[i] : (int)((seed >> 16) % 6);
		if(!garden_web_step(action)) return 1;
		puts(garden_web_digest());
	}
	return 0;
}
