#ifndef Terminal_h
#define Terminal_h

#include <signal.h>

class History;


class Terminal {
	public:
		Terminal(History* history_in);
		~Terminal();

		bool	is_done();
		int	get_terminal_fd() { return terminal_fd; }
		void	tick();
		void	send(char* data, int length = -1);
		void	hang_up();

	private:
		History* history;
		int terminal_fd;
		pid_t child_pid;
		char*	buffer;
		int prebuffered_bytes;
		static bool child_died;

		void	exec_shell();
		static void	sigchld_received(int signal_number);
	};


#endif 	// !Terminal_h


