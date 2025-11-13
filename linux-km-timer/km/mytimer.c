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

MODULE_LICENSE("Dual BSD/GPL");

// timer struct
struct ktimer{
	struct timer_list tmr; //to be able to reference specific timers more easily
	unsigned long expires; //expiration date (using jiffies)
	char msg[129]; //msg (no longer than 128) + \0
	bool active; 
};

//REFERENCE for all timer-related functions and usages:
//https://yannik520.github.io/linux_driver_code/timer/timer_example.html

static struct ktimer timers[5]; //list of all structs (max of 5)
static int max_active = 1; //start allowing only 1, but can be highered using -m (like the example video)
// lock to make sure quick consecutive read/writes dont mess up the read/writes in progress
// reference: https://www.geeksforgeeks.org/linux-unix/mutex-lock-for-linux-thread-synchronization/ 
static DEFINE_MUTEX(timers_lock); 

/* Declaration of mytimer versions of memory.c functions */
static int mytimer_open(struct inode *inode, struct file *filp);
static int mytimer_release(struct inode *inode, struct file *filp);
static ssize_t mytimer_read(struct file *filp, 
		char *buf, size_t count, loff_t *f_pos);
static ssize_t mytimer_write(struct file *filp,
		const char *buf, size_t count, loff_t *f_pos);
static void mytimer_exit(void);
static int mytimer_init(void);

struct file_operations mytimer_fops = {
	read: mytimer_read,
	      write: mytimer_write,
	      open: mytimer_open,
	      release: mytimer_release
};

/* declaration of the init & exit functions */
module_init(mytimer_init);
module_exit(mytimer_exit);

static int mytimer_major = 61;

/* buffer to store data */
static char *mytimer_buffer;
/* length of current message */
static int mytimer_len;

static int mytimer_init(void)
{
	int result;

	/* registering device */
	result = register_chrdev(mytimer_major, "mytimer", &mytimer_fops);
	if (result < 0)
	{
		printk(KERN_ALERT
				"mytimer: cannot obtain major number %d\n", mytimer_major);
		return result;
	}

	int i;
	for(i = 0; i < 5; i++)
	{
		memset(&timers[i], 0, sizeof(timers[i])); //set buffers for the timers (max 5)
	}

	printk(KERN_ALERT "Inserting mytimer module\n");
	return 0;

fail:
	mytimer_exit();
	return result;
}

static void mytimer_exit(void)
{
	/* freeing the major number */
	unregister_chrdev(mytimer_major, "mytimer");

	/* freeing buffer mem */
	int i = 0;
	for(i = 0; i < 5; i++)
	{
		if(timers[i].active)
		{
			del_timer_sync(&timers[i].tmr); //reference: https://manpages.debian.org/wheezy-backports/linux-manual-3.16/del_timer_sync.9
			timers[i].active = false;
		}
	}

	printk(KERN_ALERT "Removing mytimer module\n");
}

static int mytimer_open(struct inode *inode, struct file *filp)
{
	return 0;
}

static int mytimer_release(struct inode *inode, struct file *filp)
{
	return 0;
}

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

static ssize_t mytimer_read(struct file *filp, char *buf,
		size_t count, loff_t *f_pos)
{
	char *line = NULL;
	struct list_item items[5];
	unsigned long now = jiffies; //https://litux.nl/mirror/kerneldevelopment/0672327201/ch10lev1sec3.html

	if(*f_pos > 0)
	{
		return 0;
	}

	//populate items with active timers and remaining time left
	mutex_lock(&timers_lock);
	int i = 0;
	int n = 0;
	for(i = 0; i < 5; i++)
	{
		if(timers[i].active)
		{
			long this_rem = (long)(timers[i].expires - now);
			items[n].rem = this_rem / HZ;
			strlcpy(items[n].msg, timers[i].msg, sizeof(items[n].msg));
			n++;
		}
	}
	mutex_unlock(&timers_lock);

	//if n == 0 (no active timers exist, return 0 and print nothing
	if(n == 0)
	{
		return 0;
	}

	// if there are active timers, then sort them by remaining time left
	// helper function coompare_item to sort list_item by earliest expiration time
	sort(items, n, sizeof(items[0]), compare_item, NULL);

	// build output line <MSG> <SEC>
	line = kmalloc(n*160, GFP_KERNEL);
	line[0] = '\0';

	size_t msg_len = 0; //for keeping track of message length

	for(i = 0; i < n; i++)
	{
		char l[129];
		int l_chars = scnprintf(l, sizeof(l), "%s %ld\n", items[i].msg, items[i].rem);

		memcpy(line + msg_len, l, l_chars);
		msg_len += l_chars;
	}


	if(count > msg_len)
	{
		count = msg_len;
	}
	if(copy_to_user(buf, line, count))
	{
		kfree(line);
		return -EFAULT;
	}
	kfree(line);

	/* changing reading position as best suits */
	*f_pos += count;
	return count;
}

//msg printer helper function to output when timer expires
static void ktimer_callback(struct timer_list *t)
{
	struct ktimer *kt = from_timer(kt, t, tmr);
	printk(KERN_INFO "%s\n", kt->msg);

	mutex_lock(&timers_lock);
	kt->active = false;
	mutex_unlock(&timers_lock);
}

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

static ssize_t mytimer_write(struct file *filp, const char *buf,
		size_t count, loff_t *f_pos)
{
	char *mytimer_buffer;
	int r = 0; //what will be returned later

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

	// what I am thinking is to write a SET (-s flag) or MAX (-m flag)
	// this will be the first 3 characters followed by the rest of the write, such as:
	// SET <SEC> <MSG>
	// MAX <COUNT>
	//
	// So first, parse the first 3 characters, decide if it is a -s or -m, 
	// then do the respective things.
	
	char *firstword = mytimer_buffer;
	//find the starting point of the firstword (SET or MAX)
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
		
		//get msg
		char msg_buf[129];
		strlcpy(msg_buf, msg_start, sizeof(msg_buf));

		mutex_lock(&timers_lock);
		r = add_or_reset_timer(sec, msg_buf);
		mutex_unlock(&timers_lock);
		if(r == 0) //adding the timer succeeded, then return count
			r = count;

	}

	long max_n;
	char *count_start;

	//similar to SET, check now for MAX (-m)
	if((strncmp(firstword, "MAX", 3) == 0) && (firstword[3] == ' ' || firstword[3] == '\t'))
	{
		count_start = firstword + 3;
		//make sure there are no spaces or tabs in the beginning
		while(*count_start == ' ' || *count_start == '\t')
			count_start++;
		
		kstrtol(count_start, 10, &max_n);
		
		mutex_lock(&timers_lock);
		max_active = (int)max_n;
		mutex_unlock(&timers_lock);

		r = count;
	}
	
	kfree(mytimer_buffer);

	return r;
}

	
