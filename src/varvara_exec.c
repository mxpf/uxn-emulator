#define _POSIX_C_SOURCE 200809L

#include "varvara_internal.h"

#if defined(_WIN32)

void
varvara_exec_set_allowed(Varvara *varvara, bool allowed)
{
	varvara->exec_allowed = allowed;
}

bool
varvara_exec_running(const Varvara *varvara)
{
	(void)varvara;
	return false;
}

void
varvara_exec_close(Varvara *varvara)
{
	varvara->exec_state = NULL;
}

uint8_t
varvara_exec_read(Varvara *varvara, uint8_t port)
{
	return varvara->uxn.devices[port];
}

bool
varvara_exec_write_byte(Varvara *varvara, uint8_t byte)
{
	(void)varvara;
	(void)byte;
	return false;
}

void
varvara_exec_poll(Varvara *varvara)
{
	(void)varvara;
}

void
varvara_exec_start(Varvara *varvara)
{
	if(varvara->exec_allowed)
		fprintf(varvara->standard_error,
			"Varvara: Console/exec is unavailable on this host\n");
	else
		fprintf(varvara->standard_error,
			"Varvara: Console/exec denied; restart with --allow-exec\n");
	varvara->uxn.devices[0x15] = 0xff;
	varvara->uxn.devices[0x16] = varvara->exec_allowed ? 127 : 126;
}

#else

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

enum {
	CONSOLE_LIVE = 0x15,
	CONSOLE_EXIT = 0x16,
	CONSOLE_ADDR = 0x1c,
	CONSOLE_MODE = 0x1e
};

typedef struct {
	pid_t process;
	int input;
	int output;
	uint8_t mode;
} VarvaraExec;

static VarvaraExec *
exec_state(const Varvara *varvara)
{
	return (VarvaraExec *)varvara->exec_state;
}

static void
close_descriptor(int *descriptor)
{
	if(*descriptor >= 0) {
		close(*descriptor);
		*descriptor = -1;
	}
}

static void
free_if_finished(Varvara *varvara)
{
	VarvaraExec *state = exec_state(varvara);
	if(state && state->process == 0 && state->input < 0 && state->output < 0) {
		free(state);
		varvara->exec_state = NULL;
	}
}

static void
check_process(Varvara *varvara)
{
	VarvaraExec *state = exec_state(varvara);
	int status;
	pid_t result;
	if(!state || state->process == 0) return;
	result = waitpid(state->process, &status, WNOHANG);
	if(result != state->process) return;
	state->process = 0;
	varvara->uxn.devices[CONSOLE_LIVE] = 0xff;
	if(WIFEXITED(status))
		varvara->uxn.devices[CONSOLE_EXIT] = (uint8_t)WEXITSTATUS(status);
	else if(WIFSIGNALED(status))
		varvara->uxn.devices[CONSOLE_EXIT] =
			(uint8_t)(128 + WTERMSIG(status));
	else
		varvara->uxn.devices[CONSOLE_EXIT] = 0xff;
	close_descriptor(&state->input);
}

void
varvara_exec_set_allowed(Varvara *varvara, bool allowed)
{
	varvara->exec_allowed = allowed;
}

bool
varvara_exec_running(const Varvara *varvara)
{
	const VarvaraExec *state = exec_state(varvara);
	return state && (state->process != 0 || state->output >= 0);
}

void
varvara_exec_close(Varvara *varvara)
{
	VarvaraExec *state = exec_state(varvara);
	int status;
	if(!state) return;
	if(state->process != 0) {
		(void)kill(state->process, SIGKILL);
		while(waitpid(state->process, &status, 0) < 0 && errno == EINTR) {}
	}
	close_descriptor(&state->input);
	close_descriptor(&state->output);
	free(state);
	varvara->exec_state = NULL;
}

uint8_t
varvara_exec_read(Varvara *varvara, uint8_t port)
{
	check_process(varvara);
	return varvara->uxn.devices[port];
}

bool
varvara_exec_write_byte(Varvara *varvara, uint8_t byte)
{
	VarvaraExec *state = exec_state(varvara);
	ssize_t result;
	if(!state || !(state->mode & 0x01) || state->input < 0)
		return false;
	do {
		result = write(state->input, &byte, 1);
	} while(result < 0 && errno == EINTR);
	if(result < 0 && errno == EPIPE)
		close_descriptor(&state->input);
	return true;
}

void
varvara_exec_poll(Varvara *varvara)
{
	VarvaraExec *state = exec_state(varvara);
	uint8_t bytes[256];
	ssize_t count;
	ssize_t i;
	if(!state) return;
	if(state->output >= 0) {
		do {
			count = read(state->output, bytes, sizeof(bytes));
			if(count > 0)
				for(i = 0; i < count && !varvara_is_halted(varvara); i++)
					(void)varvara_console_input(varvara, bytes[i], 1, 0);
		} while(count > 0);
		if(count == 0 || (count < 0 && errno != EAGAIN && errno != EWOULDBLOCK))
			close_descriptor(&state->output);
	}
	check_process(varvara);
	state = exec_state(varvara);
	if(state && state->process == 0 && state->output >= 0) {
		do {
			count = read(state->output, bytes, sizeof(bytes));
			if(count > 0)
				for(i = 0; i < count && !varvara_is_halted(varvara); i++)
					(void)varvara_console_input(varvara, bytes[i], 1, 0);
		} while(count > 0);
		if(count == 0 || (count < 0 && errno != EAGAIN && errno != EWOULDBLOCK))
			close_descriptor(&state->output);
	}
	free_if_finished(varvara);
}

void
varvara_exec_start(Varvara *varvara)
{
	uint8_t mode = varvara->uxn.devices[CONSOLE_MODE];
	uint16_t address = varvara_peek_short(&varvara->uxn.devices[CONSOLE_ADDR]);
	const char *command = (const char *)&varvara->uxn.ram[address];
	VarvaraExec *state;
	int to_child[2] = {-1, -1};
	int from_child[2] = {-1, -1};
	pid_t process;

	varvara_exec_close(varvara);
	varvara->uxn.devices[CONSOLE_LIVE] = 0;
	varvara->uxn.devices[CONSOLE_EXIT] = 0;
	if(mode & 0x08) return;
	if(!varvara->exec_allowed) {
		fprintf(varvara->standard_error,
			"Varvara: Console/exec denied; restart with --allow-exec\n");
		varvara->uxn.devices[CONSOLE_LIVE] = 0xff;
		varvara->uxn.devices[CONSOLE_EXIT] = 126;
		return;
	}
	if(!memchr(command, '\0', UXN_RAM_SIZE - address)) {
		fprintf(varvara->standard_error,
			"Varvara: Console/exec command is not terminated\n");
		varvara->uxn.devices[CONSOLE_LIVE] = 0xff;
		varvara->uxn.devices[CONSOLE_EXIT] = 127;
		return;
	}
	if((mode & 0x01) && pipe(to_child) != 0)
		goto pipe_error;
	if((mode & 0x06) && pipe(from_child) != 0)
		goto pipe_error;
	state = calloc(1, sizeof(*state));
	if(!state) goto pipe_error;
	state->input = -1;
	state->output = -1;
	state->mode = mode;
	process = fork();
	if(process < 0) {
		free(state);
		goto pipe_error;
	}
	if(process == 0) {
		if(mode & 0x01) {
			(void)signal(SIGPIPE, SIG_DFL);
			(void)dup2(to_child[0], STDIN_FILENO);
			close(to_child[0]);
			close(to_child[1]);
		}
		if(mode & 0x06) {
			if(mode & 0x02) (void)dup2(from_child[1], STDOUT_FILENO);
			if(mode & 0x04) (void)dup2(from_child[1], STDERR_FILENO);
			close(from_child[0]);
			close(from_child[1]);
		}
		execl("/bin/sh", "sh", "-c", command, (char *)NULL);
		_exit(127);
	}
	state->process = process;
	if(mode & 0x01) {
		(void)signal(SIGPIPE, SIG_IGN);
		close(to_child[0]);
		state->input = to_child[1];
	}
	if(mode & 0x06) {
		int flags;
		close(from_child[1]);
		state->output = from_child[0];
		flags = fcntl(state->output, F_GETFL, 0);
		if(flags >= 0) (void)fcntl(state->output, F_SETFL, flags | O_NONBLOCK);
	}
	varvara->exec_state = state;
	varvara->uxn.devices[CONSOLE_LIVE] = 1;
	return;

pipe_error:
	close_descriptor(&to_child[0]);
	close_descriptor(&to_child[1]);
	close_descriptor(&from_child[0]);
	close_descriptor(&from_child[1]);
	fprintf(varvara->standard_error, "Varvara: Console/exec could not start\n");
	varvara->uxn.devices[CONSOLE_LIVE] = 0xff;
	varvara->uxn.devices[CONSOLE_EXIT] = 127;
}

#endif
