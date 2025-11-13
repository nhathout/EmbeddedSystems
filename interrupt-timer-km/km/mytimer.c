#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/errno.h>
#include <linux/types.h>
#include <linux/proc_fs.h>
#include <linux/fcntl.h>
#include <asm/system_misc.h>
#include <linux/uaccess.h>
#include <asm/uaccess.h>

#include <linux/timer.h>
#include <linux/jiffies.h>
#include <linux/mutex.h>
#include <linux/string.h>
#include <linux/sort.h>

//lab3 includes
//reference: “Creating an entry in /proc file system”: pointer-overloading.blogspot.com/2013/09/linux-creating-entry-in-proc-file.html
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
//reference for sched.h (for get_task_comm)
#include <linux/sched.h>

MODULE_LICENSE("Dual BSD/GPL");
MODULE_DESCRIPTION("myTimer Lab3 Kernel Module");
MODULE_AUTHOR("Noah Jean-Pierre Hathout");

// timer struct
struct ktimer{
	struct timer_list tmr; //to be able to reference specific timers more easily
	unsigned long expires; //expiration time (using jiffies)
	char msg[129]; //msg (no longer than 128) + \0
	int active; // 1-active, 0-not 

	//lab3 additions
	pid_t pid; //PID for new km requirements
	char command[TASK_COMM_LEN]; //CMD name of the userspace program that requested timer
};

//(left from lab2) REFERENCE for all timer-related functions and usages:
//https://yannik520.github.io/linux_driver_code/timer/timer_example.html

static struct ktimer tim; 
//(from lab 2, don't need for single-timer implementation) 
//static int max_active = 1; //start allowing only 1, but can be highered using -m (like the example video)

//structure for keeping track of asynchronous readers (from fasync_example.c)
struct fasync_struct *async_queue;

static unsigned long mod_load_msec; // [MSEC]
 
//from fortune.c:
static struct proc_dir_entry *proc_entry;

/* declaration of /proc/mytimer functions */
/* if cat /proc/timer -> 
 * [MODULE_NAME]
 * [MSEC]
 * [PID]
 * [CMD]
 * [SEC]
 */

/* function to print on cat command call -> open */
static int mytimer_proc_show(struct seq_file *m, void *v)
{
	unsigned long now = jiffies;
	unsigned long msec = jiffies_to_msecs(now - mod_load_msec); //https://blogs.oracle.com/linux/post/jiffies-the-heartbeat-of-the-linux-operating-system

	seq_printf(m, "[MODULE_NAME] mytimer\n");
	seq_printf(m, "[MSEC] %lu\n", msec);

	//if there is a timer registered, print it, if not print nothing
	if(tim.active == 1)
	{
		seq_printf(m, "[PID] %d\n", tim.pid);
		seq_printf(m, "[CMD] %s\n", tim.command);
		
		long rem;
		long sec;

		rem = (long)(tim.expires - now);
		sec = rem / HZ;
		seq_printf(m, "[SEC] %ld\n", sec);
	}
	return 0;
}

/* function to open proc & call proc_show */
static int mytimer_proc_open(struct inode* inode, struct file* filp)
{
	//single_open & single_release
	//reference: https://docs.kernel.org/filesystems/seq_file.html
	return single_open(filp, mytimer_proc_show, NULL);
}

//from fortune.c, no need to be able to write "not necessarily writeable"
static const struct file_operations mytimer_proc_fops = {
	.owner = THIS_MODULE,
	.open = mytimer_proc_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

/* Declaration of mytimer versions of memory.c functions */
static int mytimer_open(struct inode *inode, struct file *filp);
static int mytimer_release(struct inode *inode, struct file *filp);
static ssize_t mytimer_read(struct file *filp, 
		char *buf, size_t count, loff_t *f_pos);
static ssize_t mytimer_write(struct file *filp,
		const char *buf, size_t count, loff_t *f_pos);
static void mytimer_exit(void);
static int mytimer_init(void);

//lab3 addition
static int mytimer_fasync(int fd, struct file *filp, int mode);

struct file_operations mytimer_fops = {
	read: mytimer_read,
	      write: mytimer_write,
	      open: mytimer_open,
	      release: mytimer_release,
	      fasync: mytimer_fasync,
};

/* declaration of the init & exit functions */
module_init(mytimer_init);
module_exit(mytimer_exit);

static int mytimer_major = 61;

/* buffer to store data */
static char *mytimer_buffer;
/* length of current message */
static int mytimer_len;

//new fasync function based on fortune.c
static int mytimer_fasync(int fd, struct file *filp, int mode)
{
	return fasync_helper(fd, filp, mode, &async_queue);
}

static int mytimer_init(void)
{
	int result;
	
	memset(&tim, 0, sizeof(tim));
	mod_load_msec = jiffies; // to get time since module loaded ([MSEC])
	
	/* registering device */
	result = register_chrdev(mytimer_major, "mytimer", &mytimer_fops);
	if (result < 0)
	{
		printk(KERN_ALERT
				"mytimer: cannot obtain major number %d\n", mytimer_major);
		return result;
	}

	printk(KERN_ALERT "Inserting mytimer module\n");
	
	proc_entry = proc_create("mytimer", 0644, NULL, &mytimer_proc_fops);
	if(proc_entry == NULL){
		printk(KERN_INFO "mytimer: Couldn't create proc entry\n");
		return -ENOMEM;
	}

	return 0;
}

static void mytimer_exit(void)
{
	/* freeing the major number */
	unregister_chrdev(mytimer_major, "mytimer");
	remove_proc_entry("mytimer", NULL);
	
	// stop timer if active
	if(tim.active){
		del_timer_sync(&tim.tmr);
		tim.active = 0;
	}

	printk(KERN_ALERT "Removing mytimer module\n");
}

static int mytimer_open(struct inode *inode, struct file *filp)
{
	return 0;
}

static int mytimer_release(struct inode *inode, struct file *filp)
{
	mytimer_fasync(-1, filp, 0);
	return 0;
}

/*
//helper struct to match msg with remaining seconds till expiration
struct list_item
{
	char msg[129];
	long rem;
};


static int compare_item(const void *first, const void *second)
{
	const struct list_item *a = first;
	const struct list_item *b = second;

	// if first expires first, then return -1
	// if second expires first, then return 1
	// used link below:
	// "Returns An int value which is positive if the first string is greater, negative if the second string is greater and 0 if the two strings are equal."
	// https://www.w3schools.com/c/ref_string_strcmp.php for strcmp
	if(a->rem < b->rem) return -1;
	if(a->rem > b->rem) return 1;

	return strcmp(a->msg, b->msg); //fallback to compare messages if they have the same rem time
}
*/

static ssize_t mytimer_read(struct file *filp, char *buf,
		size_t count, loff_t *f_pos)
{
	char line[150]; // space for 129 from [MSG] + extra for [SEC] 
	size_t len = 0;
	
	if(*f_pos > 0)
	{
		return 0;
	}

	//populate line with active timer and remaining time left
	if(tim.active)
	{
		long rem = (long)(tim.expires - jiffies);
		long sec = rem / HZ;
		// len = amount of chars printed successfully
		len = scnprintf(line, sizeof(line), "%s %ld\n", tim.msg, sec);
	}

	if(len == 0) //no timer
	{
		return 0;
	}

	if(count > len)
	{
		count = len;
	}

	if(copy_to_user(buf, line, count)){
		return -EFAULT;
	}

	/* changing reading position as best suits */
	*f_pos += count;
	return count;
}

// fasync implementation; referenced km/fasync_example.c
static void timer_handler(struct timer_list *data)
{
	tim.active = 0;

	kill_fasync(&async_queue, SIGIO, POLL_IN);
}

/*
//helper function to add/reset timer given SEC & MSG, 
//also uses jiffies and calls another helper for callback
static int add_or_reset_timer(long sec, const char *msg)
{
	unsigned long expire_time = jiffies + sec * HZ;
	
	//check if msg already exists, if it does get the position in timers[]
	int pos = -1; // -1 unless it exists in timers[]
	int i = 0;
	for(i = 0; i < 5; i++)
	{
		if(timers[i].active && (strcmp(timers[i].msg, msg) == 0)){
			pos = i;
		}
	}

	if(pos >= 0) //timer did already exist
	{
		// reset it
		timers[pos].expires = expire_time;
		mod_timer(&timers[pos].tmr, expire_time);
		return 0;
	}

	//timer didnt exist so make a new timer
	//first count # active timers and see if we have the space to add another
	
	int j = 0;
	int numt = 0;
	for(j = 0; j < 5; j++)
	{
		if(timers[j].active)
		{
			numt++;
		}
	}

	if(numt >= max_active)
		return -ENOSPC;

	for(i = 0; i < 5; i++)
	{
		//go until there is a space for a new timer (timer isn't active)
		//then make the new timer there
		if(!timers[i].active)
		{
			timers[i].active = true;
			strlcpy(timers[i].msg, msg, sizeof(timers[i].msg));
			timers[i].expires = expire_time;

			timer_setup(&timers[i].tmr, ktimer_callback, 0);
			
			timers[i].tmr.expires = expire_time;
			add_timer(&timers[i].tmr);
			return 0;
		}
	}

	return -ENOSPC;
}
*/

static ssize_t mytimer_write(struct file *filp, const char *buf,
		size_t count, loff_t *f_pos)
{
	char *mytimer_buffer;

	mytimer_buffer = kmalloc(count + 1, GFP_KERNEL);

	if (copy_from_user(mytimer_buffer, buf, count))
	{
		kfree(mytimer_buffer);
		return -EFAULT;
	}

	mytimer_buffer[count] = '\0';

	// get rid of \n
	if(count > 0)
	{
		char *nl = strrchr(mytimer_buffer, '\n'); //strrchr gets last occurrence of \n
		if(nl)
		{
			*nl = '\0'; //change \n to \0
		}
	}

	// what I am thinking is to write a SET (-s flag) or DEL (-r flag)
	// this will be the first 3 characters followed by the rest of the write, such as:
	// SET <SEC> <MSG>
	// DEL 
	//
	// So first, parse the first 3 characters, decide if it is a -s or -m, 
	// then do the respective things.
	
	char *firstword = mytimer_buffer;
	//find the starting point of the firstword (SET or DEL)
	while(*firstword == ' ' || *firstword == '\t' || *firstword == '\n')
		firstword++;

	//check for SET (-s)
	//then check for 4 character (in case MSG is SETTING or something)
	if((strncmp(firstword, "SET", 3) == 0) && (firstword[3] == ' ' || firstword[3] == '\t' || firstword[3] == '\n'))
	{
		char *sec_start = firstword + 3; //the timer length is 3 characters ahead of the start of SET
		while(*sec_start == ' ' || *sec_start == '\t')
			sec_start++; //get to actual beginning of number (without spaces or tabs in front)

		//get sec
		long sec;
		char *sec_end = sec_start;
		while(*sec_end && *sec_end != ' ' && *sec_end != '\t') //get to actual end of number
			sec_end++;

		char temp = *sec_end;
		*sec_end = '\0'; //temp so kstrtol works correctly
		kstrtol(sec_start, 10, &sec); //reference: https://manpages.debian.org/testing/linux-manual-4.8/kstrtol.9.en.html

		*sec_end = temp;
		char *msg_start = sec_end;

		while(*msg_start == ' ' || *msg_start == '\t') //get to actual start of message
			msg_start++;
		
		tim.active = 0;
		tim.pid = current->pid;
		get_task_comm(tim.command, current);
		strlcpy(tim.msg, msg_start, sizeof(tim.msg));
		tim.expires = jiffies + (unsigned long)sec * HZ;

		//if timer exists, modify it
		//if not, setup it up
		if(timer_pending(&tim.tmr))
		{
			mod_timer(&tim.tmr, tim.expires);
		}else{
			timer_setup(&tim.tmr, timer_handler, 0);
			tim.tmr.expires = tim.expires;
			add_timer(&tim.tmr);
		}
		
		//state active
		tim.active = 1;

		kfree(mytimer_buffer);
		return count;
	}

	if(strncmp(firstword, "DEL", 3) == 0)
	{
		//if timer active, remove it (no prints)
		tim.active = 0;
		// USEFUL LINK: https://embetronicx.com/tutorials/linux/device-drivers/using-kernel-timer-in-linux-device-driver/#
		// timer_pending checks if the timer is on/pending/turning off/etc
		if(timer_pending(&tim.tmr)){
			del_timer_sync(&tim.tmr);
		}
		kfree(mytimer_buffer);
		return count;
	}

	/*
	long max_n;
	char *count_start;’s -l query can be implemented by either reading the character device fi
	
	//similar to SET, check now for MAX (-m)
	if((strncmp(firstword, "MAX", 3) == 0) && (firstword[3] == ' ' || firstword[3] == '\t'))
	{
		count_start = firstword + 3;
		//make sure there are no spaces ’s -l query can be implemented by either reading the character device fior tabs in the beginning
		while(*count_start == ' ' || *count_start == '\t')
			count_start++;
		
		kstrtol(count_start, 10, &max_n);
		
		mutex_lock(&timers_lock);’s -l query can be implemented by either reading the character device fi
		max_active = (int)max_n;
		mutex_unlock(&timers_lock);

		r = count;
	}
	*/

	kfree(mytimer_buffer);

	return count;
}
