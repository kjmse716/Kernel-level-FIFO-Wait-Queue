#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/syscalls.h>

static int condition = 0;
DECLARE_WAIT_QUEUE_HEAD(my_wait_queue);
static DEFINE_MUTEX(my_wait_queue_lock);
static int enter_wait_queue(void)
{
    condition = 0;
    DECLARE_WAITQUEUE(wait_entry,current);
    mutex_lock(&my_wait_queue_lock); // 加鎖
    add_wait_queue_exclusive(&my_wait_queue, &wait_entry);
    mutex_unlock(&my_wait_queue_lock); // 加鎖
    printk("before join wait queue,  %d",current->pid);
    printk("wait queue before join new one: ");

    wait_event_interruptible_exclusive(my_wait_queue,condition);
    printk("after waiting event,  %d",current->pid);
    remove_wait_queue(&my_wait_queue,&wait_entry);
    if(waitqueue_active(&my_wait_queue)!=0)
        wake_up_interruptible_sync(&my_wait_queue);

    return 0;
}
static int clean_wait_queue(void)
{
    printk("start waking up process.");
    mutex_lock(&my_wait_queue_lock); // 加鎖
    condition = 1;
    mutex_unlock(&my_wait_queue_lock); // 加鎖
    if(waitqueue_active(&my_wait_queue)!=0)
        wake_up_interruptible_sync(&my_wait_queue);
    printk("wake clear");
    return 0;
}
SYSCALL_DEFINE1(call_my_wait_queue, int, id)
{
switch (id){
case 1:
    enter_wait_queue();
    break;
case 2:
    clean_wait_queue();
    break;
}
return 0;
}
