#include "Terminal.h"
#include "History.h"
#include "Settings.h"
#include <pty.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <pwd.h>
#include <time.h>
#include <stdexcept>


// Because we can't pass a parameter to the signal handler, we can't really
// handle more than one terminal in our process without adding extra mechanism.
bool Terminal::child_died = false;


Terminal::Terminal(History* history_in)
	: history(history_in), child_pid(0)
{
	buffer = (char*) malloc(BUFSIZ);
	prebuffered_bytes = 0;

	int child_fd;
	int result = openpty(&terminal_fd, &child_fd, NULL, NULL, NULL);
	if (result < 0)
		throw std::runtime_error("openpty() failed");

	pid_t pid = fork();
	if (pid < 0)
		throw std::runtime_error("fork() failed");
	else if (pid == 0) {
		// We're in the child.
		setsid();	// Create a new process group.
		dup2(child_fd, 0);
		dup2(child_fd, 1);
		dup2(child_fd, 2);
		if (ioctl(child_fd, TIOCSCTTY, NULL) < 0)
			throw std::runtime_error("ioctl(TIOCSCTTY) failed");
		close(child_fd);
		close(terminal_fd);
		exec_shell();
		}
	else {
		child_pid = pid;
		close(child_fd);
		struct sigaction sigchld_action;
		memset(&sigchld_action, 0, sizeof(sigchld_action));
		sigchld_action.sa_handler = sigchld_received;
		sigaction(SIGCHLD, &sigchld_action, NULL);
		}
}


Terminal::~Terminal()
{
	free(buffer);
}


bool Terminal::is_done()
{
	return child_died;
}


void Terminal::tick()
{
	// We assume this won't be called unless select() has indicated there is
	// data to read.

	if (child_died)
		return;

	// Read.
	int result =
		read(terminal_fd, buffer + prebuffered_bytes, BUFSIZ - prebuffered_bytes);
	if (result == 0) {
		child_died = true;
		return;
		}
	else if (result < 0) {
		// Wait a moment before checking child_died again.
		struct timespec sleep_spec = { 0, 1000000 };
		nanosleep(&sleep_spec, nullptr);
		if (child_died) {
			// This is a race condition; the child died *while* we were reading.
			return;
			}
		throw std::runtime_error("read() failed");
		}

	// Give it to the History.
	int length = prebuffered_bytes + result;
	int bytes_consumed = history->add_input(buffer, length);
	length -= bytes_consumed;
	if (length > 0)
		memmove(buffer, buffer + bytes_consumed, length);
	prebuffered_bytes = length;
}


void Terminal::send(const char* data, int length)
{
	if (length == -1)
		length = strlen(data);
	// st makes sure not to send more than 256 bytes at a time, because it might
	// be connected to a modem.  We don't bother with that (currently).
	while (length > 0) {
		ssize_t bytes_written = write(terminal_fd, data, length);
		if (bytes_written < 0)
			throw std::runtime_error("write() failed");
		data += bytes_written;
		length -= bytes_written;
		}
}


void Terminal::hang_up()
{
	if (child_pid > 0)
		kill(child_pid, SIGHUP);
}


void Terminal::notify_resize(int columns, int rows, int pixel_width, int pixel_height)
{
	struct winsize window_size;
	window_size.ws_row = rows;
	window_size.ws_col = columns;
	window_size.ws_xpixel = pixel_width;
	window_size.ws_ypixel = pixel_height;
	ioctl(terminal_fd, TIOCSWINSZ, &window_size);
}


void Terminal::exec_shell()
{
	const struct passwd* user_info = getpwuid(getuid());
	if (user_info == NULL)
		throw std::runtime_error("Couldn't get user info");

	// Which shell to use.
	const char* shell_path = getenv("SHELL");
	if (shell_path == NULL) {
		if (user_info->pw_shell[0])
			shell_path = user_info->pw_shell;
		else
			shell_path = "/bin/bash";
		}

	// Environment.
	unsetenv("COLUMNS");
	unsetenv("LINES");
	unsetenv("TERMCAP");
	setenv("LOGNAME", user_info->pw_name, true);
	setenv("USER", user_info->pw_name, true);
	setenv("SHELL", shell_path, true);
	setenv("HOME", user_info->pw_dir, true);
	setenv("TERM", settings.term_name.c_str(), true);
	setenv("SPFT", "true", true);

	// Signals.
	signal(SIGCHLD, SIG_DFL);
	signal(SIGHUP, SIG_DFL);
	signal(SIGINT, SIG_DFL);
	signal(SIGQUIT, SIG_DFL);
	signal(SIGTERM, SIG_DFL);
	signal(SIGALRM, SIG_DFL);

	// Working directory.
	if (!settings.working_directory.empty()) {
		std::string path = settings.working_directory;
		if (path[0] == '~')
			path = settings.home_path() + path.substr(1);
		chdir(path.c_str());
		}

	// Start the shell.
	char* args[] = { (char*) shell_path, 0 };
	execvp(shell_path, args);
}


void Terminal::sigchld_received(int signal_number)
{
	child_died = true;
}



