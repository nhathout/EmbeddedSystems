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
 * 		-m [COUNT]		(change # of active timers supported by kernel module)
 *
 * Examples:
 * 	./ktimer -l 
 * 		List to stdout the expiration time [SEC] and the message [MSG] that would be printed upon timer expiration
 * 		example output should be <MSG> <SEC>
 *
 * 	./ktimer -s [SEC] [MSG]
 * 		Register a new timer that, after [SEC] seconds will print the message [MSG] (no print if successful)
 * 		If an active timer with the same [MSG] already exists, then reset the timer's remaining time to [SEC] and print:
 * 		The timer <MSG> was updated!
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

// variable to track max number of active timers (-m flag)
static int current_max = 1;

int main(int argc, char **argv){
	char line[256];
	int ii, count = 0;

	/* check to see if the mytimer successfully has mknod run 
	 * Assumes that mytimer is tied to /dev/mytimer */
	FILE *pFile;
	pFile = fopen("/dev/mytimer", "r+");
	if (pFile == NULL) {
		fputs("mytimer module isn't loaded\n", stderr);
		return -1;
	}

	// check for -l flag
	if (strcmp(argv[1], "-l") == 0){
		while(fgets(line, sizeof(line), pFile))
		{
			printf("%s", line);
		}
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

		char msg[129]; // [128 + NULL]
		msg[0] = '\0';
		
		// build [MSG]
		size_t used = 0; // variable to keep track of how many characters used in msg
		for(int i = 3; i < argc; i++){
			const char *part = argv[i];
			size_t amt_used = strlen(part);
			
			// put space if it isnt the first word
			if(used && used + 1 < sizeof(msg))
			{
				msg[used++] = ' ';
			}
			
			memcpy(msg + used, part, amt_used); // copy one world e.x. "Hello"
			used += amt_used;
			
			msg[used] = '\0'; 

			// failsafe for if we filled msg
			if(used >= 128)
			{
				break;
			}
		}

		int already_exists = msg_exists(msg); 

		snprintf(line, sizeof(line), "SET %ld %s\n", sec, msg);
		
		// check if current_max was exceeded
		if(fputs(line, pFile) == EOF)
		{
			int err = errno;
			if(err == ENOSPC)
			{
				printf("A timer already exists!\n", current_max);
				return 1;
			}
			perror("fputs");
			return 1;
		}

		//make sure to finish the write with fflush
		if(fflush(pFile) != 0)
		{
			int err = errno;
			if(err == ENOSPC)
			{
				printf("A timer already exists!\n", current_max);
				return 1;
			}
			perror("flush");
			return 1;
		}

		if(already_exists)
		{
			printf("The timer %s was updated!\n", msg);
		}
	}

	// check for -m flag
	else if (argc == 3 && strcmp(argv[1], "-m") == 0){
		// same way I did before, get [COUNT]
		char *n = NULL;
		long count = strtol(argv[2], &n, 10);
		
		fprintf(pFile, "MAX %ld\n", count);

		current_max = (int)count;
	}

	// otherwise invalid
	else {
		printManPage();
	}

	fclose(pFile);
	return 0;
}

void printManPage(){
	printf("Error: invalid use.\n");
	printf(" ktimer [-flag]\n");
	printf(" -l: list all active timers\n");
	printf(" -s [SEC] [MSG]: register new timer to print [MSG] after [SEC] seconds\n");
	printf(" -m [COUNT]: change # of active timers to [COUNT]\n");
}

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
