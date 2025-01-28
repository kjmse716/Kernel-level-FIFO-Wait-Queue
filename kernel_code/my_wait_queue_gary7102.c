#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/wait.h>
#include <linux/syscalls.h>
#include <linux/spinlock.h>
#include <linux/list.h>
#include <linux/delay.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Gary Chen");
MODULE_DESCRIPTION("Custom Wait Queue Implementation");

// 定義結構
typedef struct wait_queue_node {
    struct list_head list;
    struct task_struct *task;
} wait_queue_node_t;

// 定義全域變數
static LIST_HEAD(my_wait_queue); // 自定義的 FIFO Queue
static spinlock_t queue_lock;

// 初始化模組
static int __init my_wait_queue_init(void)
{
    spin_lock_init(&queue_lock);
    printk(KERN_INFO "Wait Queue Module Initialized\n");
    return 0;
}

// 清理模組
static void __exit my_wait_queue_exit(void)
{
    printk(KERN_INFO "Wait Queue Module Exited\n");
}

// 加入 Wait Queue
static int enter_wait_queue(void)
{
    wait_queue_node_t *node;

    // 分配新節點
    node = kmalloc(sizeof(wait_queue_node_t), GFP_KERNEL);
    if (!node)
        return 0;

    node->task = current;

    spin_lock(&queue_lock);
    list_add_tail(&node->list, &my_wait_queue); // 加入 FIFO Queue
    printk(KERN_INFO "Thread ID %d added to wait queue\n", current->pid);
    spin_unlock(&queue_lock);

    set_current_state(TASK_INTERRUPTIBLE); // 設定為可中斷的睡眠狀態
    schedule(); // 進入睡眠

    return 1; // 成功加入
}

// 清理 Wait Queue
static int clean_wait_queue(void)
{
    wait_queue_node_t *node, *temp;

    spin_lock(&queue_lock);
    list_for_each_entry_safe(node, temp, &my_wait_queue, list) {
        list_del(&node->list); // 從 Queue 中移除

        printk(KERN_INFO "Thread ID %d removed from wait queue\n", node->task->pid);

        wake_up_process(node->task); // 喚醒進程
        kfree(node); // 釋放記憶體
  	
	msleep(100); // sleep 100 ms
    }
    spin_unlock(&queue_lock);

    return 1; // 成功清理
}

// 系統呼叫接口
SYSCALL_DEFINE1(call_my_wait_queue, int, id)
{
    int result = 0;
    switch (id)
    {
    case 1:
        result = enter_wait_queue();
        break;
    case 2:
        result = clean_wait_queue();
        break;
    default:
        printk(KERN_ERR "Invalid ID for wait queue operation\n");
        result = 0;
        break;
    }
    return result;
}

module_init(my_wait_queue_init);
module_exit(my_wait_queue_exit);
