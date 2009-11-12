/* Signal handling setup and code.

	Copyright (C) 1993-1998 Sebastiano Vigna 
	Copyright (C) 1999-2009 Todd M. Lewis and Sebastiano Vigna

	This file is part of ne, the nice editor.

	This program is free software; you can redistribute it and/or modify it
	under the terms of the GNU General Public License as published by the
	Free Software Foundation; either version 2, or (at your option) any
	later version.
	
	This program is distributed in the hope that it will be useful, but
	WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
	General Public License for more details.
	
	You should have received a copy of the GNU General Public License along
	with this program; see the file COPYING.  If not, write to the Free
	Software Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
	02111-1307, USA.  */

#ifndef _AMIGA

#include "ne.h"
#include <signal.h>



/* These variables remember if we are already in a signal handling
code. In this case, the arrival of another signal must kill us. */

static int fatal_code_in_progress, fatal_error_code;



/* This code is called by all the fatal signals. It records that something
bad is happening, and then tries to autosave all the files currently in
memory using auto_save(). If another signal arrives during the execution,
we exit without any other delay. */

static void fatal_code(const int sig) {

	fatal_error_code = sig;

	signal (sig, SIG_DFL);

	if (fatal_code_in_progress) kill(getpid (), fatal_error_code);

	fatal_code_in_progress = TRUE;

	/* Let us clean up the terminal configuration. */

	unset_interactive_mode();

	apply_to_list(&buffers, auto_save);

	kill(getpid (), fatal_error_code);
}


/* The next two functions handle the suspend/restart system. When stopped,
we reset the terminal status, set up the continuation handler and let the
system stop us by sending again a TSTP signal, this time using the default
handler. When we get a CONT signal, we set the terminal, set up the stop
handler and reset the screen. */

/*static void cont_code(int sig) {

	set_interactive_mode();
	ttysize();
	}*/

void stop_ne(void) {
	unset_interactive_mode();
	kill(getpid(), SIGTSTP);
	set_interactive_mode();
	clear_entire_screen();
	ttysize();
	reset_window();
}



/* This mask will hold all the existing signals. */

static sigset_t signal_full_mask;


/* Diverts to fatal_code() the behaviour of all fatal signals.  Moreover,
   signal_full_mask is filled with all the existing signals.

   PORTABILITY PROBLEM: certain systems could have extra, non-POSIX signals
   whose trapping could be necessary. Feel free to add other signals to this
   list, but please leave SIGINT for the interrupt character. */

void set_fatal_code(void) {

	sigfillset (&signal_full_mask);

	signal(SIGALRM, fatal_code);
	signal(SIGILL, fatal_code);
	signal(SIGABRT, fatal_code);
	signal(SIGFPE, fatal_code);
	signal(SIGSEGV, fatal_code);
	signal(SIGTERM, fatal_code);
	signal(SIGHUP, fatal_code);
	signal(SIGQUIT, fatal_code);
	signal(SIGPIPE, fatal_code);
	signal(SIGUSR1, fatal_code);
	signal(SIGUSR2, fatal_code);
	signal(SIGTTIN, fatal_code);
}


/* This variable keeps track of the degree of nesting of blocking
and releasing signals, so that block_signals() and release_signals()
can be safely called at any time. */


static int signal_block_nest_count;

/* The following functions block and release, respectively, all signals
(except of course the ones which cannot be trapped). They are used in order
to make atomic sections which modify vital parts of the internal state, such
as lists. */


void block_signals(void) {


	if (!signal_block_nest_count++)
		sigprocmask(SIG_BLOCK, &signal_full_mask, NULL);
}


void release_signals(void) {

	if (!--signal_block_nest_count)
		sigprocmask(SIG_UNBLOCK, &signal_full_mask, NULL);
}



/* Handles SIGQUIT. It just sets the stop global variable to TRUE, so that the
   interested functions can check it, and restores itself as signal handler. */

void set_stop (const int sig) {
	signal(sig, SIG_IGN);
	stop = TRUE;
	signal(sig, set_stop);
}

/* Handles SIGINT. It just restores itself as signal handler. */

void handle_int (const int sig) {
	signal(sig, handle_int);
}

/* Handles SIGWINCH, if present. It calls ttysize(). */

#ifdef SIGWINCH
void handle_winch (const int sig) {
	signal(sig, SIG_IGN);
	window_changed_size = ttysize();
	signal(sig, handle_winch);
}
#endif

#endif

unsigned int window_changed_size;
unsigned int stop;