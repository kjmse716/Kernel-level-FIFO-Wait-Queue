# <font color = "orange">Intro</font>
<font size = 4>**2024 Fall NCU Linux OS Project 2**</font>
* Implement a custom wait queue-like functionality in kernel space. Allowing user applications to operate through the system call.
* Add a thread to the wait queue to wait. Remove threads from the wait queue, allowing them exit in **FIFO** order

```
113522008 陳國誌 gary7102
113522006 李秉睿
113522053 蔡尚融 kjmse716 
```

**Link :**  [Project_2](https://github.com/kjmse716/NCU-Linux-OS-2024-LAB2/blob/main/Project_2.pdf) , [github](https://github.com/kjmse716/NCU-Linux-OS-2024-LAB2/tree/main) , [hackmd](https://hackmd.io/@kjmse716/r1M-pcJIye)


<font size = 4>**Environment**</font>
```
OS: Ubuntu 22.04 on VMware workstation 17
ARCH: X86_64
Kernel Version: 5.15.137
```


# <font color = "orange">Wait Queue Implemention</font>

<font size = 4>**以下為kernel space code**</font>

<font size = 4>**wait queue node**</font>
```c
typedef struct wait_queue_node {
    struct list_head list;
    struct task_struct *task;
} wait_queue_node_t;
```
* 每個node存表示 `my_wait_queue` 中的一個節點，每個節點對應一個process，裡面存了:
    * `list_head list`：使用 `list_head`(Circular doubly linked list）將所有等待的process接在一起。
    * `task_struct *task`: 每個process 指向自己的 process descriptor

<font size = 4>**global var.**</font>
```c
static LIST_HEAD(my_wait_queue); // self-define FIFO Queue
static spinlock_t queue_lock;    // protect list_add, list_del
```
* `my_wait_queue`： 使用 `LIST_HEAD` 定義了一個空的circularly doubly linked list，作為head node of wait queue。
* `queue_lock`：使用 `spinlock_t` 定義一個spin lock，用於保護multi-threads對 `my_wait_queue` 的操作，防止race condition。
    * 一次就只會有一個thread 取得鑰匙，目的就是要保護`my_wait_queue` ，同時間只會有一個thread 在操作，包含 delete, add 都不能同時間執行

<font size = 4>**enter_wait_queue**</font>
```c
static int enter_wait_queue(void)
{
    wait_queue_node_t *node;

    // 分配新節點
    node = kmalloc(sizeof(wait_queue_node_t), GFP_KERNEL);
    if (!node)
        return 0;

    node->task = current;
    
    spin_lock(&queue_lock);
    ////////// Critical Region ///////////////////
    list_add_tail(&node->list, &my_wait_queue); // 加入 FIFO Queue
    printk(KERN_INFO "Thread ID %d added to wait queue\n", current->pid);
    ////////// Critical Region End ///////////////
    spin_unlock(&queue_lock);

    set_current_state(TASK_INTERRUPTIBLE); // 設定為可中斷的睡眠狀態
    schedule(); // 進入睡眠

    return 1; // 成功加入
}
```
* 使用 `spin_lock` 保護`list_add_tail`，同時間只會有一個node 加入 `my_wait_queue` ，確保不會出現race condition。
* 使用 `list_add_tail` 將節點加入queue尾部。
* 設置process state為 `TASK_INTERRUPTIBLE`，並使用 `schedule()` 將當前process 設為睡眠，直到呼叫 `wake_up_process` 才可以離開wait queue。  

:::warning
記得 `schedule();` 要加在spinlock之外，  
否則在 critical region 中使用 `schedule();` 會使 process 占用 lock 不放，導致deadlock 發生
:::


<font size = 4>**clean_wait_queue**</font>
```c
static int clean_wait_queue(void)
{
    wait_queue_node_t *node, *temp;

    spin_lock(&queue_lock);
    ////////// Critical Region ///////////////////
    list_for_each_entry_safe(node, temp, &my_wait_queue, list) {
        list_del(&node->list); // 從 Queue 中移除

        printk(KERN_INFO "Thread ID %d removed from wait queue\n", node->task->pid);

        wake_up_process(node->task); // 喚醒進程
        kfree(node); // 釋放記憶體

        msleep(100); // sleep 100 ms
    }
    ////////// Critical Region End ///////////////
    spin_unlock(&queue_lock);

    return 1; // 成功清理
}
```
* 使用 `list_for_each_entry_safe` 遍歷wait queue
    * 使用 `list_del` 從queue中移除節點。
    * 使用 `wake_up_process` 喚醒process。
    * 釋放節點的記憶體。

:::success
這邊加入 `msleep(100)`   
因為在user space 中印出的順序受到multi-thread的影響可能會亂掉  
因此即使kernel space 的wait queue已經確保是FIFO的順序移除，但是在User space I/O 卻無法保證照順序印出，  
所以在`wake_up_process`之後加上睡眠100ms，讓user space  有時間可以印出資訊而不被多執行緒打亂 
:::


# <font color = "orange">Function explanation</font>

<font size = 4>**list_head**</font>
```c
/*
 * Circular doubly linked list implementation.
 *
 * Some of the internal functions ("__xxx") are useful when
 * manipulating whole lists rather than single entries, as
 * sometimes we already know the next/prev entries and we can
 * generate better code by using them directly rather than
 * using the generic single-entry routines.
 */
#define LIST_HEAD_INIT(name) { &(name), &(name) }

#define LIST_HEAD(name) \
	struct list_head name = LIST_HEAD_INIT(name)
```
可以看到這是一個 Circular doubly linked list , 也就是我們要用來儲存wait queue的資料結構，  
使用 `LIST_HEAD` 就定義了一個空的雙向鏈表，作為等待Queue的head節點。


    
<font size = 4>**list_add_tail**</font>
```c

/*
 * Insert a new entry between two known consecutive entries.
 *
 * This is only for internal list manipulation where we know
 * the prev/next entries already!
 */
static inline void __list_add(struct list_head *new,
			      struct list_head *prev,
			      struct list_head *next)
{
	if (!__list_add_valid(new, prev, next))
		return;

	next->prev = new;
	new->next = next;
	new->prev = prev;
	WRITE_ONCE(prev->next, new);
}

// ...

/**
 * list_add_tail - add a new entry
 * @new: new entry to be added
 * @head: list head to add it before
 *
 * Insert a new entry before the specified head.
 * This is useful for implementing queues.
 */
static inline void list_add_tail(struct list_head *new, struct list_head *head)
{
	__list_add(new, head->prev, head);
       // = add   (new, tail-node , head)
}
```
* `head` 是circular linked list的 head節點
* 透過 `head->prev` 獲取尾節點，並將新節點插入到尾節點和頭節點之間，也就是加入linked list最後面。
* `__list_add` 中，其實就是將`new` 插入到`prev`和`next`當中，透過4個操作:
    * 更新 `next->prev = new` 和 `new->next = next`
    * 更新 `new->prev = prev` 和 `prev->next = new`

**圖例:**
```markdown
# circularly linked list
before add: next <-> A <-> B <-> C <->prev

# 加入new node到最後面:
after add : next <-> A <-> B <-> C <-> prev <-> new
```

<font size = 4>**list_del**</font>

```c
/*
 * Delete a list entry by making the prev/next entries point to each other.
 * This is only for internal list manipulation where we know
   the prev/next entries already!
 */
static inline void __list_del(struct list_head * prev, struct list_head * next)
{
	next->prev = prev;
	WRITE_ONCE(prev->next, next);
}


/*
 * Check validity of deletion
 * */
static inline void __list_del_entry(struct list_head *entry)
{
	if (!__list_del_entry_valid(entry))
		return;

	__list_del(entry->prev, entry->next);
}


/**
 * list_del - deletes entry from list.
 * @entry: the element to delete from the list.
 * Note: list_empty() on entry does not return true after this, the entry is
 * in an undefined state.
 */
static inline void list_del(struct list_head *entry)
{
	__list_del_entry(entry);
	entry->next = LIST_POISON1;  // (void *) 0x00100100, invalid addr.
	entry->prev = LIST_POISON2;  // (void *) 0x00200200, invalid addr.
}
```
* 從雙向鏈表（`list_head`）中刪除指定的節點。
    * 透過 `next->prev = prev` 及 `prev->next = next` 達到delete node
* 使用 `LIST_POISON1` 及 `LIST_POISON2` 明確標記已刪除的節點，便於檢測誤用


**圖例:**
```markdown
# circularly linked list
before delete: head <-> A <-> prev <-> dnode <-> next

# Delete dnode:
after delete : head <-> A <-> prev <-> next
```


<font size = 4>**list_for_each_entry_safe**</font>
```c
/**
 * list_for_each_entry_safe - iterate over list of given type safe against removal of list entry
 * @pos:	the type * to use as a loop cursor.
 * @n:		another type * to use as temporary storage
 * @head:	the head for your list.
 * @member:	the name of the list_head within the struct.
 */
#define list_for_each_entry_safe(pos, n, head, member)			\
	for (pos = list_first_entry(head, typeof(*pos), member),	\
		n = list_next_entry(pos, member);			\
	     !list_entry_is_head(pos, head, member); 			\
	     pos = n, n = list_next_entry(n, member))
```
* `pos` 保存當前節點，`n` 保存`pos` 的下一個節點（通常用存放在`tmp`）。
* `n` 確保當前節點(`pos`)刪除後，仍然能獲取下一個節點的地址。
* 刪除 `pos` 後，會透過 `pos=n` 及 `n=list_next_entry()` 一棟到下一個node，因此可以確保刪除的正確性．

# <font color = "orange">**User space code**</font>

:::spoiler `wait_queue.c`
```c
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/syscall.h>
#define NUM_THREADS 10

#define SYS_CALL_MY_WAIT_QUEUE 452

void *enter_wait_queue(void *thread_id)
{
	pid_t tid = syscall(SYS_gettid); // get thread id
	fprintf(stderr, "enter wait queue thread_id: %d, current thread id = %d\n", *(int *)thread_id, tid);	
	int result;	// 
	result = syscall(SYS_CALL_MY_WAIT_QUEUE, 1);
	
	fprintf(stderr, "exit wait queue thread_id: %d, current thread id = %d\n", *(int *)thread_id, tid);
}
void *clean_wait_queue()
{

	int result;	//
	result = syscall(SYS_CALL_MY_WAIT_QUEUE, 2);
}

int main()
{
	void *ret;
	pthread_t id[NUM_THREADS];
	int thread_args[NUM_THREADS];

	for (int i = 0; i < NUM_THREADS; i++)
 	{
 		thread_args[i] = i;
 		pthread_create(&id[i], NULL, enter_wait_queue, (void *)&thread_args[i]);
 	}
 	sleep(1);
        
        fprintf(stderr, "start clean queue ...\n");
	
	clean_wait_queue();
 	
	for (int i = 0; i < NUM_THREADS; i++)
 	{
 		pthread_join(id[i], &ret);
 	}
	 return 0;
}
```
:::

# 執行結果
<font size = 4>__User space result:__</font>

![image](https://hackmd.io/_uploads/S1VaJs18kl.png)



透過原本的User space code for迴圈中 `i` 值以及真正的thread id來檢查，  
進入wait queue以及離開wait queue的順序是保持FIFO


<font size = 4>__Kernel space result:__</font>

![image](https://hackmd.io/_uploads/Sy3a1o18yx.png)





# Another implementation

:::spoiler my_wait_queue.c
```c=1
#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/syscalls.h>
#include <linux/delay.h>

static int condition = 0;
DECLARE_WAIT_QUEUE_HEAD(my_wait_queue);
static DEFINE_MUTEX(my_wait_queue_lock);
static int enter_wait_queue(void)
{   
    condition = 0;
    DECLARE_WAITQUEUE(wait_entry,current);
    mutex_lock(&my_wait_queue_lock); // 加鎖
    add_wait_queue_exclusive(&my_wait_queue, &wait_entry);
    printk("before join wait queue,  %d",current->pid);

    wait_event_exclusive_cmd(my_wait_queue,condition,mutex_unlock(&my_wait_queue_lock);,mutex_lock(&my_wait_queue_lock););
    printk("after waiting event,  %d",current->pid);
    remove_wait_queue(&my_wait_queue,&wait_entry);
    if(waitqueue_active(&my_wait_queue)!=0){
        msleep(100);
        wake_up(&my_wait_queue);    
    }
    mutex_unlock(&my_wait_queue_lock); // 加鎖
    return 0;
}
static int clean_wait_queue(void)
{
    printk("start waking up process.");
    mutex_lock(&my_wait_queue_lock); // 加鎖
    condition = 1;
    if(waitqueue_active(&my_wait_queue)!=0)
        wake_up(&my_wait_queue);
    mutex_unlock(&my_wait_queue_lock); // 加鎖
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
```

:::

## 全域、static變數初始化

```c=6
static int condition = 0;
DECLARE_WAIT_QUEUE_HEAD(my_wait_queue);
static DEFINE_MUTEX(my_wait_queue_lock);
```
此區塊會初始化變數
`6：`首先將用於`wait_event()`判斷process是否擁有所需的資源的`condition`變數設為0(告訴wait_event目前process無法取得所需資源，需要進行wait)。  

`7：`透過`DECLARE_WAIT_QUEUE_HEAD()`來建立一個作為全域變數的wait queue head實例：`my_wait_queue`。  

`8：`DEFINE一個static的Mutex lock實例：`my_wait_queue_lock`。

## enter_wait_queue() 
```c=9
static int enter_wait_queue(void)
{   
    condition = 0;
    DECLARE_WAITQUEUE(wait_entry,current);
    mutex_lock(&my_wait_queue_lock); // 加鎖
    add_wait_queue_exclusive(&my_wait_queue, &wait_entry);
    printk("before join wait queue,  %d",current->pid);

    wait_event_exclusive_cmd(my_wait_queue,condition,mutex_unlock(&my_wait_queue_lock);,mutex_lock(&my_wait_queue_lock););
    printk("after waiting event,  %d",current->pid);
    remove_wait_queue(&my_wait_queue,&wait_entry);
    if(waitqueue_active(&my_wait_queue)!=0){
        msleep(100);
        wake_up(&my_wait_queue);    
    }
    mutex_unlock(&my_wait_queue_lock); // 加鎖
    return 0;
}
```
`11：`將`condition`設回0以確保想加入wait queue的process能夠正確進入wait狀態。  

`12：`透過`wait.h`中的`DECLARE_WAITQUEUE()` macro來建立一個新的wait queue entry。  

`13：`加鎖以確保進行`add_wait_queue()`操作時不會有競爭條件 (Race Condition)的情況發生。  

`14：`透過`wait.h`中的function `add_wait_queue_exclusive()`來將process加入wait queue。之所以會選擇exclusive加入的原因一方面是讓之後可以直接使用`wake_up()`就可以每次叫醒一個process進行執行(減少規定不能改動的user space code中printf順序出錯的機率)，另一方面是考量`add_wait_queue_exclusive()`本身會將要進入wait_queue的process串接在wait_queue最後方的性質，正好符合這次題目FIFO的需求。  

`17：`透過同樣於`wait.h`中的function`wait_event_exclusive_cmd()`來讓process進入wait狀態(因為沒有所需資源(condition=0)，故讓process wait)，而最後面的兩個cmd分別會於process開始wait前;process被叫醒時執行，這裡設定於開始wait前將mutex lock解鎖，並於被叫醒後嘗試獲得mutex lock。<font color=red> **當process進入wait狀態後實際上被暫停在這行**  </font>

`18：`<font color=green>當process被叫醒後會由這行繼續執行(得到mutex lock後)。</font>  

`19：`透過`wait.h`中的function `remove_wait_queue()`將process由wait queue中移除。  

`20~23：`讓此process檢查，假設waitqueue中還有其他在wait的process，就將其叫醒(同樣是應對題目不可修改的Userspace code中printf順續亂掉的問題)。  


`24：`在此被叫醒的process返回userspace繼續執行前將mutex lock解鎖讓剛剛wake_up()的其他process可以開始執行。  


## clean_wait_queue()
```c=27
static int clean_wait_queue(void)
{
    printk("start waking up process.");
    mutex_lock(&my_wait_queue_lock); // 加鎖
    condition = 1;
    if(waitqueue_active(&my_wait_queue)!=0)
        wake_up(&my_wait_queue);
    mutex_unlock(&my_wait_queue_lock); // 加鎖
    return 0;
}
```
此function內的實作與上方`enter_wait_queue()`中負責叫醒其他process的`19~24`行相同，主要用於叫醒第一個process。


## 執行結果：
### UserSpace output：
![image](https://hackmd.io/_uploads/SJhFSC1UJe.png)

### KernelSpace output：

![image](https://hackmd.io/_uploads/HkbqHRJLkl.png)







# Reference

* [Waitqueue in Linux](https://embetronicx.com/tutorials/linux/device-drivers/waitqueue-in-linux-device-driver-tutorial/)

