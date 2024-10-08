// Copyright (c) Athena Dev Teams - Licensed under GNU GPL
// For more information, see LICENCE in the main folder

#include <stdio.h>
#include <stdlib.h>
#ifndef _WIN32
#include <unistd.h>
#endif
#include <signal.h>
#include <string.h>

#include "core.h"
#include "../common/mmo.h"
#include "../common/version.h"
#include "../common/showmsg.h"
#include "../common/malloc.h"
#ifndef MINICORE
#include "../common/db.h"
#include "../common/socket.h"
#include "../common/timer.h"
#include "../common/plugins.h"
#endif

#ifndef _WIN32
	#include "svnversion.h"
#endif

int runflag = 1;
int arg_c = 0;
char **arg_v = NULL;

char *SERVER_NAME = NULL;
char SERVER_TYPE = ATHENA_SERVER_NONE;
#ifndef SVNVERSION
	static char eA_svn_version[10];
#endif

#ifndef MINICORE	// minimalist Core
// Added by Gabuzomeu
//
// This is an implementation of signal() using sigaction() for portability.
// (sigaction() is POSIX; signal() is not.)  Taken from Stevens' _Advanced
// Programming in the UNIX Environment_.
//
#ifdef WIN32	// windows don't have SIGPIPE
#define SIGPIPE SIGINT
#endif

#ifndef POSIX
#define compat_signal(signo, func) signal(signo, func)
#else
sigfunc *compat_signal(int signo, sigfunc *func)
{
	struct sigaction sact, oact;

	sact.sa_handler = func;
	sigemptyset(&sact.sa_mask);
	sact.sa_flags = 0;
#ifdef SA_INTERRUPT
	sact.sa_flags |= SA_INTERRUPT;	/* SunOS */
#endif

	if (sigaction(signo, &sact, &oact) < 0)
		return (SIG_ERR);

	return (oact.sa_handler);
}
#endif

/*======================================
 *	CORE : Signal Sub Function
 *--------------------------------------*/
static void sig_proc(int sn)
{
	static int is_called = 0;

	switch (sn) {
	case SIGINT:
	case SIGTERM:
		if (++is_called > 3)
			exit(0);
		runflag = 0;
		break;
	case SIGSEGV:
	case SIGFPE:
		do_abort();
		// Pass the signal to the system's default handler
		compat_signal(sn, SIG_DFL);
		raise(sn);
		break;
#ifndef _WIN32
	case SIGXFSZ:
		// ignore and allow it to set errno to EFBIG
		ShowWarning ("Max file size reached!\n");
		//run_flag = 0;	// should we quit?
		break;
	case SIGPIPE:
		ShowMessage ("Broken pipe found... closing socket\n");	// set to eof in socket.c
		break;	// does nothing here
#endif
	}
}

void signals_init (void)
{
	compat_signal(SIGTERM, sig_proc);
	compat_signal(SIGINT, sig_proc);
#ifndef _DEBUG // need unhandled exceptions to debug on Windows
	compat_signal(SIGSEGV, sig_proc);
	compat_signal(SIGFPE, sig_proc);
#endif
#ifndef _WIN32
	compat_signal(SIGILL, SIG_DFL);
	compat_signal(SIGXFSZ, sig_proc);
	compat_signal(SIGPIPE, sig_proc);
	compat_signal(SIGBUS, SIG_DFL);
	compat_signal(SIGTRAP, SIG_DFL);
#endif
}
#endif

#ifdef SVNVERSION
	#define xstringify(x) stringify(x)
	#define stringify(x) #x
	const char *get_svn_revision(void)
	{
		return xstringify(SVNVERSION);
	}
#else// not SVNVERSION
const char* get_svn_revision(void)
{
	//FILE *fp;

	//if(*eA_svn_version)
	//	return eA_svn_version;

	/*if ((fp = fopen(".svn/entries", "r")) != NULL)
	{
		char line[1024];
		int rev;
		// Check the version
		if (fgets(line, sizeof(line), fp))
		{
			if(!ISDIGIT(line[0]))
			{
				// XML File format
				while (fgets(line,sizeof(line),fp))
					if (strstr(line,"revision=")) break;
				if (sscanf(line," %*[^\"]\"%d%*[^\n]", &rev) == 1) {
					snprintf(eA_svn_version, sizeof(eA_svn_version), "%d", rev);
				}
			}
			else
			{
				// Bin File format
				fgets(line, sizeof(line), fp); // Get the name
				fgets(line, sizeof(line), fp); // Get the entries kind
				if(fgets(line, sizeof(line), fp)) // Get the rev numver
				{
					snprintf(eA_svn_version, sizeof(eA_svn_version), "%d", atoi(line));
				}
			}
		}
		fclose(fp);
	}*/

	//if(!(*eA_svn_version))
		snprintf(eA_svn_version, sizeof(eA_svn_version), "Negis-RO");

	return eA_svn_version;
}
#endif

/*======================================
 *	CORE : Display title
 *--------------------------------------*/
static void display_title(void)
{
	//ClearScreen(); // clear screen and go up/left (0, 0 position in text)
	printf("\n");
	//ShowInfo("Negis eAthena Erosia 2.0 By nznick\n");
	ShowInfo("==============================================\n");
	ShowInfo("Negis (R) Core 1.0 by nznick\n");
	ShowInfo("==============================================\n");
	printf("\n");
	//ShowMessage(""CL_WTBL"				"CL_YELLOW"Negis Athena Erosia 2.0 By nznick"CL_RESET"						"CL_CLL""CL_NORMAL"");
	ShowInfo("Server By nznick\n\n"); //show credits

	ShowInfo("SVN Revision: '"CL_WHITE"%s"CL_RESET"'.\n", get_svn_revision());
}

// Warning if logged in as superuser (root)
void usercheck(void)
{
#ifndef _WIN32
    if ((getuid() == 0) && (getgid() == 0)) {
	ShowWarning ("You are running eAthena as the root superuser.\n");
	ShowWarning ("It is unnecessary and unsafe to run eAthena with root privileges.\n");
	sleep(3);
    }
#endif
}

/*======================================
 *	CORE : MAINROUTINE
 *--------------------------------------*/
int main (int argc, char **argv)
{
	{// initialize program arguments
		char *p1 = SERVER_NAME = argv[0];
		char *p2 = p1;
		while ((p1 = strchr(p2, '/')) != NULL || (p1 = strchr(p2, '\\')) != NULL)
		{
			SERVER_NAME = ++p1;
			p2 = p1;
		}
		arg_c = argc;
		arg_v = argv;
		#ifndef SVNVERSION
			*eA_svn_version = '\0';
		#endif
	}

	malloc_init();// needed for Show* in display_title() [FlavioJS]

#ifdef MINICORE // minimalist Core
	display_title();
	usercheck();
	do_init(argc,argv);
	do_final();
#else// not MINICORE
	set_server_type();
	display_title();
	usercheck();

	db_init();
	signals_init();

	timer_init();
	socket_init();
	plugins_init();

	do_init(argc,argv);
	plugin_event_trigger(EVENT_ATHENA_INIT);

	{// Main runtime cycle
		int next;
		while (runflag) {
			next = do_timer(gettick_nocache());
			do_sendrecv(next);
			do_parsepacket();
		}
	}

	plugin_event_trigger(EVENT_ATHENA_FINAL);
	do_final();

	timer_final();
	plugins_final();
	socket_final();
	db_final();
#endif

	malloc_final();

	return 0;
}

#ifdef BCHECK
unsigned int __invalid_size_argument_for_IOC;
#endif
