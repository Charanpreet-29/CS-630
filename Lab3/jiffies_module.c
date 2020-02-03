/* simple_module.c - a simple template for a loadable kernel module in Linux,
   based on the hello world kernel module example on pages 338-339 of Robert
   Love's "Linux Kernel Development, Third Edition."
 */

#include <linux/init.h>
#include <linux/timer.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/jiffies.h> 
/* init function - logs that initialization happened, returns success */
extern unsigned long volatile jiffies;
static int simple_init (void) {
 unsigned long now_tick = jiffies ; // now
     printk(KERN_ALERT "simple module initialized, time is:%lu \n",now_tick);
    return 0;
}

/* exit function - logs that the module is being removed */
static void simple_exit (void) {
	unsigned long now_tick = jiffies; // now
	printk(KERN_ALERT "simple module is being unloaded\n, time is : %lu \n",now_tick);


}

module_init (simple_init);
module_exit (simple_exit);

