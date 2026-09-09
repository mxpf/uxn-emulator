#include "../src/square_web.c"

int main(void)
{
	int op;
	while((op = getchar()) != EOF) {
		int ok = 0;
		if(op == '0') ok = no_escape_reset();
		else if(op >= '1' && op <= '4') ok = no_escape_input(op - '0');
		else if(op == '.') ok = no_escape_step();
		else if(op == 'R') ok = no_escape_replay();
		else if(op == 'V') {
			unsigned limit = 0; ok = 1;
			while(!(no_escape_flags() & 4) && ok && limit++ <= SQUARE_TURNS + 1) ok = no_escape_step();
			ok = ok && (no_escape_flags() & 4);
		} else if(op == 'x') ok = no_escape_input(256);
		else return 1;
		printf("%d %s\n", !!ok, no_escape_digest());
	}
	square_free(&session); return 0;
}
