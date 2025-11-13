#define _GNU_SOURCE
#include <signal.h>
#include <fcntl.h>
#include <unistd.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

/***********************************************************
 * Prepared by Noah Hathout
 *
 * Usage:
 * 	./ktimer [flag]
 *
 * 		-l 			(list all active timers)
 * 		-s [SEC] [MSG] 		(register new timer) 
 * 		*EXTRA* -m [COUNT]	(change # of active timers supported by kernel module)
 *
 * Examples:
 * 	./ktimer -l 
 * 		List to stdout the expiration time [SEC] and the message [MSG] that would be printed upon timer expiration
 * 		example output should be <MSG> <SEC>
 *
 * 	./ktimer -s [SEC] [MSG]
 * 		Register a new timer that, after [SEC] seconds will print the message [MSG] (no print if successful)
 * 		If kernel doesn't support +1 timer, print "Cannot add another timer!"
 * 		*EXTRA* If an active timer with the same [MSG] already exists, then reset the timer's remaining time to [SEC] and print:
 * 		The timer <MSG> was updated!
 *
 *	./ktimer -r 
 *		remove all/any timers registered
 *
 * 	./ktimer -m [COUNT] 
 * 		Change the # of active timers supported by your kernel module to [COUNT]. NO PRINT if no errors.
 * 		(default is 1)
 * 		If multiple timers are not supported, should print:
 * 		Error: multiple timers not supported.
 *
 ***********************************************************/


void printManPage(void); 
static int msg_exists(const char *msg); // function to handle case where message already exists (-s flag)

// additional helpers for lab3 (reference ul/fasync_tester.c)
void sighandler(int);
static ssize_t write_full(int dest, const void *buf, size_t l); //function to write all timer(s) info to STDOUT
static int get_active_timer(char *line, char *out, size_t outsize); //function to get current active timer for comparison

static void put(int dest, const char *s)
{
	write_full(dest, s, strlen(s));
}

// variable to track max number of active timers (-m flag)
static int current_max = 1;
static char sig_msg[129]; // message to print on SIGIO

int main(int argc, char **argv){
	if(argc < 2) // check for incorrect usage
	{
		printManPage();
		return 1;
	}

	//no fopen allowed for ul, so just going to move file opening (open()) to inside ifs

	// check for -l flag
	if (strcmp(argv[1], "-l") == 0){ 
		int pFile = open("/dev/mytimer", O_RDONLY); //important link (reference): https://man7.org/linux/man-pages/man2/open.2.html
	
		if(pFile < 0) // /dev/timer not there
		{
			return 0; // print nothing if no timers
		}

		char buf[256];
		ssize_t num_read = read(pFile, buf, sizeof(buf));
		close(pFile);
		if(num_read > 0) //there was something read
		{	
			ssize_t offset = 0;
			while(offset < num_read)
			{
				ssize_t num = write(STDOUT_FILENO, buf + offset, (size_t)(num_read - offset));
				if(num < 0)
				{
					break;
				}
				offset += num;
			}
		}

		return 0;
	}	
	
	// check for -s flag
	else if (strcmp(argv[1], "-s") == 0){
		// check for incorrect usage
		if(argc < 4){
			printManPage();
			return 1;
		}
		
		// get [SEC]
		char *n = NULL; //null pointer to see end of argv
		long sec = strtol(argv[2], &n, 10);
		
		// build [MSG]
		size_t used = 0; // variable to keep track of how many characters used in msg
		for(int i = 3; i < argc; i++){
			const char *part = argv[i];
			size_t amt_used = strlen(part);
			
			// put space if it isnt the first word
			if(used && used + 1 < sizeof(sig_msg))
			{
				sig_msg[used++] = ' ';
			}
			
			memcpy(sig_msg + used, part, amt_used); // copy one world e.x. "Hello"
			used += amt_used;
			
			sig_msg[used] = '\0'; 

			// failsafe for if we filled msg
			if(used >= 128)
			{
				break;
			}
		}
		
		//check to see if there is an active timer,
		//if there is, save its msg to compare new and active
		int has_timer = 0;
		char active_msg[129];
		active_msg[0] = '\0';

		char buf[256];

		
		
		int pFile = open("/dev/mytimer", O_RDWR);
		if(pFile < 0)
			return 0;
		
		ssize_t num_read = read(pFile, buf, sizeof(buf));
		if(num_read < 0)
			num_read = 0;
		else if(sizeof(buf))
			buf[num_read] = '\0';

		if(num_read > 0)
		{
			has_timer = get_active_timer(buf, active_msg, sizeof(active_msg));
		}
		
		// check if there is an active timer with a diff msg
		// prints: "Cannot add another timer!"
		if(has_timer && strcmp(active_msg, sig_msg) != 0)
		{
			put(STDOUT_FILENO, "Cannot add another timer!\n");
			return 0;
		}

		if(pFile < 0)
		{
			put(STDERR_FILENO, "mytimer module isn't loaded\n");
			return 1;
		}

		//check if were updating or creating a new timer
		int updating = (has_timer && strcmp(active_msg, sig_msg) == 0);
		//if were not updating (creating), then install SIGIO handler and enable async 
		if (!updating)
		{
			//reference fasync_tester.c)
			struct sigaction action;
			memset(&action, 0, sizeof(action));
			action.sa_handler = sighandler;
			action.sa_flags = SA_SIGINFO;
			sigemptyset(&action.sa_mask);
			sigaction(SIGIO, &action, NULL);

			fcntl(pFile, F_SETOWN, getpid());
			int oflags = fcntl(pFile, F_GETFL);
			fcntl(pFile, F_SETFL, oflags | FASYNC);
		}

		//send command to /dev/timer
		char cmd[256];
		int num_wrote = snprintf(cmd, sizeof(cmd), "SET %ld %s\n", sec, sig_msg);

		if(num_wrote < 0 || write_full(pFile, cmd, (size_t)num_wrote) < 0)
		{
			if(errno == ENOSPC)
			{
				put(STDOUT_FILENO, "Cannot add another timer!\n");
				return 0;
			}
			close(pFile);
			return 1;
		}

		//finally, if updating a timer w/ same msg, just write:
		// "The timer [MSG] was updated!"
		if(updating)
		{
			put(STDOUT_FILENO, "The timer ");
			write_full(STDOUT_FILENO, sig_msg, strlen(sig_msg));
			put(STDOUT_FILENO, " was updated!\n");
			return 0;
		}

		//sleep until kernel sends SIGIO
		for(;;) 
		{
			pause();
		}
	}

	// check for -m flag
	/* *EXTRA*
	else if (argc == 3 && strcmp(argv[1], "-m") == 0){
		// same way I did before, get [COUNT]
		char *n = NULL;
		long count = strtol(argv[2], &n, 10);
		
		fprintf(pFile, "MAX %ld\n", count);

		current_max = (int)count;
	}
	*/

	else if(strcmp(argv[1], "-m") == 0)
	{
		char s[64] = "Error: multiple timers not supported.\n";
		
		write_full(STDOUT_FILENO, s, strlen(s));
		return 0;
	}

	else if(strcmp(argv[1], "-r") == 0)
	{
		int dest = open("/dev/mytimer", O_WRONLY);

		if(dest >= 0)
		{
			write_full(dest, "DEL\n", 4);
			close(dest);
		}

		return 0;
	}

	// otherwise invalid
	else {
		printManPage();
	}
	return 0;
}

void printManPage(){
	printf("Error: invalid use.\n");
	printf(" ktimer [-flag]\n");
	printf(" -l: list all active timers\n");
	printf(" -s [SEC] [MSG]: register new timer to print [MSG] after [SEC] seconds\n");
	printf(" -r: remove any registered timer(s)\n");
	// *EXTRA* printf(" -m [COUNT]: change # of active timers to [COUNT]\n");
}

void sighandler(int signo)
{
	(void)signo;
	size_t msg_len = strnlen(sig_msg, 129);
	
	if(msg_len)
	{
		write_full(STDOUT_FILENO, sig_msg, msg_len);
		write_full(STDOUT_FILENO, "\n", 1);
	}
	_exit(0);
}

static ssize_t write_full(int dest, const void *buf, size_t l)
{
	const char *p = (const char*)buf;
	size_t charleft = l;
	while(charleft > 0)
	{
		ssize_t n = write(dest, p, charleft);
		if(n < 0)
		{
			if(errno == EINTR) continue;
			return -1;
		}

		p += n;
		charleft -= (size_t)n;
	}
	return (ssize_t)l;
}

static int get_active_timer(char *line, char *out, size_t outsize)
{
	if(!line)
		return 0;

	size_t line_len = strlen(line);
	//if there is a timer swap the \n with a \0
	if(line_len && line[line_len-1] == '\n')
	{
		line[line_len--] = '\0';
	}

	char *last_space = strrchr(line, ' ');
	if(!last_space)
		return 0;
	*last_space = '\0'; //line == [MSG]
	
	size_t ll = strlen(line);
	if(ll >= outsize)
	{
		ll = outsize - 1; //clip line length to outsize
	}
	memcpy(out, line, ll);
	out[ll] = '\0';
	return 1;
}

/*
static int msg_exists(const char *msg)
{
	FILE *pf;
        pf = fopen("/dev/mytimer", "r+");

	char l[256];
	size_t msglen = strlen(msg);
  	int found = 0;
 	
	while(fgets(l, sizeof(l), pf))
	{
		// lines are [MSG] [SEC] so:
		// 1. strip \n's
		// 2. find last space to continue until then

		char *nl = strchr(l, '\n');
		if(nl)
		{
			*nl = '\0';
		}

		char *last_space = strchr(l, ' ');
		if(!last_space) continue;
		*last_space = '\0';

		//finally check if l == msg (it exists)
		if(strcmp(l, msg) == 0)
		{
			found = 1;
			break;
		}
	}

	fclose(pf);
	return found;
}	
*/

