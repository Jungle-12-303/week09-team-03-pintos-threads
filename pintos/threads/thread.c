#include "threads/thread.h"
#include <debug.h>
#include <stddef.h>
#include <random.h>
#include <stdio.h>
#include <string.h>
#include "threads/flags.h"
#include "threads/interrupt.h"
#include "threads/intr-stubs.h"
#include "threads/palloc.h"
#include "threads/synch.h"
#include "threads/vaddr.h"
#include "intrinsic.h"
#ifdef USERPROG
#include "userprog/process.h"
#endif

/* Random value for struct thread's `magic' member.
   Used to detect stack overflow.  See the big comment at the top
   of thread.h for details. */
#define THREAD_MAGIC 0xcd6abf4b

/* Random value for basic thread
   Do not modify this value. */
#define THREAD_BASIC 0xd42df210

/* List of processes in THREAD_READY state, that is, processes
   that are ready to run but not actually running. */
/* 
THREAD_READY 상태의 프로세스 목록, 즉 실행 준비는 되었으나 실제로는 실행 중이 아닌 프로세스들. 
실행 준비가 되어(Unblock) CPU를 할당 받기 위해 순서를 기다리거 있는 일반적인 스레드(프로세스)들이 대기하는 리스트
-> 한정된 자원을 두고 경쟁하는 상대
*/
static struct list ready_list;

/* timer_sleep 호출로 blocked 스레드를 넣을 리스트 */
static struct list sleep_list;


/* SONNY'S CODE semaphore 구조체 변수 접근을 위한 포인터*/
// struct semaphore* sema_pointer;

/* Idle thread. */
/*
ready_list에 실행한 준비가 된 스레드가 단 하낟고 없을 때(시스템에 당장 실행할 작업이 없을 때)
스케줄러에 의해 선택되어 실행되는 특수한 스레드
-> CPU가 아무 작업도 하지 않고 멈추는 것을 방지하기 위해 빈 시간을 채워주는 역할

CPU가 놀고 있다고 Blocked 스레드를 실행하는 것은 X
-> 운영체제가 시스템을 구동하면서 CPU가 놀게 될 때를 대비하여 자체적으로 미리 만들어두는 특수한 목적의 empty 스레드
*/
static struct thread *idle_thread;

/* Initial thread, the thread running init.c:main(). */
static struct thread *initial_thread;

/* Lock used by allocate_tid(). */
static struct lock tid_lock;

/* Thread destruction requests */
// Proejct1에서 쓰이지 않는 리스트
static struct list destruction_req;

/* Statistics. */
static long long idle_ticks;    /* # of timer ticks spent idle. */
static long long kernel_ticks;  /* # of timer ticks in kernel threads. */
static long long user_ticks;    /* # of timer ticks in user programs. */

/* Scheduling. */
#define TIME_SLICE 4            /* # of timer ticks to give each thread. */
static unsigned thread_ticks;   /* # of timer ticks since last yield. */

/* If false (default), use round-robin scheduler.
   If true, use multi-level feedback queue scheduler.
   Controlled by kernel command-line option "-o mlfqs". */
bool thread_mlfqs;

static void kernel_thread (thread_func *, void *aux);

static void idle (void *aux UNUSED);
static struct thread *next_thread_to_run (void);
static void init_thread (struct thread *, const char *name, int priority);
static void do_schedule(int status);
static void schedule (void);
static tid_t allocate_tid (void);





/* Returns true if T appears to point to a valid thread. */
#define is_thread(t) ((t) != NULL && (t)->magic == THREAD_MAGIC)

/* Returns the running thread.
 * Read the CPU's stack pointer `rsp', and then round that
 * down to the start of a page.  Since `struct thread' is
 * always at the beginning of a page and the stack pointer is
 * somewhere in the middle, this locates the curent thread. */
#define running_thread() ((struct thread *) (pg_round_down (rrsp ())))


// Global descriptor table for the thread_start.
// Because the gdt will be setup after the thread_init, we should
// setup temporal gdt first.
static uint64_t gdt[3] = { 0, 0x00af9a000000ffff, 0x00cf92000000ffff };

/* Initializes the threading system by transforming the code
   that's currently running into a thread.  This can't work in
   general and it is possible in this case only because loader.S
   was careful to put the bottom of the stack at a page boundary.

   Also initializes the run queue and the tid lock.

   After calling this function, be sure to initialize the page
   allocator before trying to create any threads with
   thread_create().

   It is not safe to call thread_current() until this function
   finishes. */
/* 현재 실행 중인 코드를 하나의 스레드로 변환하여 스레딩 시스템을 초기화한다.
   일반적으로는 이런 방식이 동작할 수 없지만, 이 경우에는 loader.S가
   스택의 바닥을 페이지 경계에 맞추도록 신중하게 배치했기 때문에 가능하다.

   또한 run queue와 tid lock도 초기화한다.

   이 함수를 호출한 뒤에는 thread_create()로 스레드를 생성하려고 하기 전에
   반드시 페이지 할당자를 초기화해야 한다.

   이 함수가 끝나기 전까지는 thread_current()를 호출하는 것이 안전하지 않다. */
void
thread_init (void) {
	ASSERT (intr_get_level () == INTR_OFF);

	/* Reload the temporal gdt for the kernel
	 * This gdt does not include the user context.
	 * The kernel will rebuild the gdt with user context, in gdt_init (). */
	struct desc_ptr gdt_ds = {
		.size = sizeof (gdt) - 1,
		.address = (uint64_t) gdt
	};
	lgdt (&gdt_ds);

	/* Init the globla thread context */
	lock_init (&tid_lock);
	list_init (&ready_list);
	// sleep_list 초기화 
	list_init (&sleep_list);
	list_init (&destruction_req);

	/* Set up a thread structure for the running thread. */
	initial_thread = running_thread ();
	init_thread (initial_thread, "main", PRI_DEFAULT);
	initial_thread->status = THREAD_RUNNING;
	initial_thread->tid = allocate_tid ();
}

/* Starts preemptive thread scheduling by enabling interrupts.
   Also creates the idle thread. */
void
thread_start (void) {
	/* Create the idle thread. */
	struct semaphore idle_started;
	sema_init (&idle_started, 0);
	thread_create ("idle", PRI_MIN, idle, &idle_started);

	/* Start preemptive thread scheduling. */
	intr_enable ();

	/* Wait for the idle thread to initialize idle_thread. */
	sema_down (&idle_started);
}

/* Called by the timer interrupt handler at each timer tick.
   Thus, this function runs in an external interrupt context. */
/* 타이머 인터럽트 핸들러에 의해 매 타이머 틱마다 호출된다.
   따라서 이 함수는 외부 인터럽트 컨텍스트에서 실행된다. */
void
thread_tick (void) {
	struct thread *t = thread_current ();

	/* Update statistics. */
	if (t == idle_thread)
		idle_ticks++;
#ifdef USERPROG
	else if (t->pml4 != NULL)
		user_ticks++;
#endif
	else
		kernel_ticks++;

	/* Enforce preemption. */
	if (++thread_ticks >= TIME_SLICE)
		intr_yield_on_return ();
}

/* Prints thread statistics. */
void
thread_print_stats (void) {
	printf ("Thread: %lld idle ticks, %lld kernel ticks, %lld user ticks\n",
			idle_ticks, kernel_ticks, user_ticks);
}

/* Creates a new kernel thread named NAME with the given initial
   PRIORITY, which executes FUNCTION passing AUX as the argument,
   and adds it to the ready queue.  Returns the thread identifier
   for the new thread, or TID_ERROR if creation fails.

   If thread_start() has been called, then the new thread may be
   scheduled before thread_create() returns.  It could even exit
   before thread_create() returns.  Contrariwise, the original
   thread may run for any amount of time before the new thread is
   scheduled.  Use a semaphore or some other form of
   synchronization if you need to ensure ordering.

   The code provided sets the new thread's `priority' member to
   PRIORITY, but no actual priority scheduling is implemented.
   Priority scheduling is the goal of Problem 1-3. */
/* 주어진 초기 PRIORITY를 가지며 NAME이라는 이름의 새 커널 스레드를 생성한다.
   이 스레드는 AUX를 인자로 전달받아 FUNCTION을 실행하며,
   생성된 뒤 ready queue에 추가된다. 새 스레드의 thread identifier를
   반환하거나, 생성에 실패하면 TID_ERROR를 반환한다.

   thread_start()가 호출된 상태라면, 새 스레드는 thread_create()가
   반환되기 전에 스케줄링될 수도 있다. 심지어 thread_create()가
   반환되기 전에 종료될 수도 있다. 반대로, 새 스레드가 스케줄링되기 전에
   기존 스레드가 얼마든지 오랫동안 실행될 수도 있다.
   실행 순서를 보장해야 한다면 세마포어 또는 다른 형태의 동기화를 사용하라.

   제공된 코드는 새 스레드의 `priority' 멤버를 PRIORITY로 설정하지만,
   실제 우선순위 스케줄링은 구현되어 있지 않다.
   우선순위 스케줄링은 Problem 1-3의 목표이다. */
tid_t
thread_create (const char *name, int priority, thread_func *function, void *aux) {
	struct thread *t; // 새로 만들어질 스레드의 포인터
	tid_t tid; // 새로 만들어질 스레드의 ID

	ASSERT (function != NULL);

	/* Allocate thread. */
	t = palloc_get_page (PAL_ZERO); 
	if (t == NULL)
		return TID_ERROR;

	/* Initialize thread. */
	init_thread (t, name, priority);
	tid = t->tid = allocate_tid (); 

	/* Call the kernel_thread if it scheduled.
	 * Note) rdi is 1st argument, and rsi is 2nd argument. */
	t->tf.rip = (uintptr_t) kernel_thread;
	t->tf.R.rdi = (uint64_t) function; // kernel_thread의 첫 번째 인자 function을 rdi에 넣어줌 실행 아님
	t->tf.R.rsi = (uint64_t) aux;
	t->tf.ds = SEL_KDSEG;
	t->tf.es = SEL_KDSEG;
	t->tf.ss = SEL_KDSEG;
	t->tf.cs = SEL_KCSEG;
	t->tf.eflags = FLAG_IF;

	/* Add to run queue. */
	thread_unblock (t); 

	/* NICK - thread_unblock(t) 안에서 ready_list에 우선순위에 맞게 넣어주기 때문에 thread_create에서는 따로 우선순위 비교해서 넣어줄 필요 X */
	if (t -> priority > thread_current () -> priority) /* t는 새로 만들어진 스레드, thread_current()는 현재 실행 중인 스레드 */
		thread_yield (); /* 새 스레드가 우선순위가 높다면 thred_yield*()를 호출해서 현재 스레드가 cpu에서 양보하도록 함 */

	return tid;
	/* NICK */
}

/* Puts the current thread to sleep.  It will not be scheduled
   again until awoken by thread_unblock().

   This function must be called with interrupts turned off.  It
   is usually a better idea to use one of the synchronization
   primitives in synch.h. */
/* 현재 스레드를 잠들게 한다. 이 스레드는 thread_unblock()에 의해
   다시 깨어나기 전까지는 다시 스케줄링되지 않는다.

   이 함수는 반드시 인터럽트가 꺼진 상태에서 호출되어야 한다.
   일반적으로는 synch.h에 있는 동기화 기본 요소들 중 하나를
   사용하는 것이 더 좋은 방법이다. */
void
thread_block (void) {
	ASSERT (!intr_context ());
	ASSERT (intr_get_level () == INTR_OFF);
	thread_current ()->status = THREAD_BLOCKED;
	schedule ();
}
/* Transitions a blocked thread T to the ready-to-run state.
   This is an error if T is not blocked. (Use thread_yield() to make the running thread ready.)

   This function does not preempt the running thread.
   This can be important: if the caller had disabled interrupts itself,
   it may expect that it can atomically unblock a thread and update other data. */
/* 블록된 스레드 T를 실행 준비 상태(ready-to-run state)로 전환한다.
   T가 블록된 상태가 아니라면 이는 오류이다. 실행 중인 스레드를
   준비 상태로 만들려면 thread_yield()를 사용하라.

   이 함수는 현재 실행 중인 스레드를 선점하지 않는다.
   이 점이 중요할 수 있다. 호출자가 직접 인터럽트를 비활성화했다면,
   스레드 하나를 원자적으로 unblock하고 다른 데이터를 갱신할 수
   있기를 기대할 수 있기 때문이다. */
void
thread_unblock (struct thread *t) {
	enum intr_level old_level;

	ASSERT (is_thread (t));

	old_level = intr_disable ();
	ASSERT (t->status == THREAD_BLOCKED);

	/* SONNY'S CODE */
	list_insert_ordered (&ready_list, &t->elem, compare_priority, NULL);
	/* SONNY'S CODE */

	t->status = THREAD_READY; //
	intr_set_level (old_level); 
}

/* Returns the name of the running thread. */
const char *
thread_name (void) {
	return thread_current ()->name;
}

/* Returns the running thread.
   This is running_thread() plus a couple of sanity checks.
   See the big comment at the top of thread.h for details. */
struct thread *
thread_current (void) {
	struct thread *t = running_thread ();

	/* Make sure T is really a thread.
	   If either of these assertions fire, then your thread may
	   have overflowed its stack.  Each thread has less than 4 kB
	   of stack, so a few big automatic arrays or moderate
	   recursion can cause stack overflow. */
	ASSERT (is_thread (t));
	ASSERT (t->status == THREAD_RUNNING);

	return t;
}

/* Returns the running thread's tid. */
tid_t
thread_tid (void) {
	return thread_current ()->tid;
}

/* Deschedules the current thread and destroys it.  Never
   returns to the caller. */
void
thread_exit (void) {
	ASSERT (!intr_context ());

#ifdef USERPROG
	process_exit ();
#endif

	/* Just set our status to dying and schedule another process.
	   We will be destroyed during the call to schedule_tail(). */
	intr_disable ();
	do_schedule (THREAD_DYING);
	NOT_REACHED ();
}

/* Yields the CPU.  The current thread is not put to sleep and
   may be scheduled again immediately at the scheduler's whim. */
/* CPU를 양보합니다. 현재 스레드는 일시 정지되지 않으며,
   스케줄러의 판단에 따라 즉시 다시 스케줄링될 수 있습니다. */
void
thread_yield (void) {
	struct thread *curr = thread_current ();
	enum intr_level old_level;
	ASSERT (!intr_context ());
	old_level = intr_disable ();

	/* NICK - ready_list에 현재 스레드를 우선순위에 맞게 넣어주기 
	   &ready_list는 cpu에 할당받기 위해 준비된 스레드들이 대기하는 리스트이므로 우선순위에 맞게 큐에 
	   &curr->elem는 현재 스레드의 list_elem 구조체로, ready_list에 삽입될 때 사용됨
	   compare_priority는 우선순위 비교 함수, NULL은 보조 인자(사용하지 않으므로 NULL) */
	if (curr != idle_thread)
		list_insert_ordered (&ready_list, &curr->elem, compare_priority, NULL);

	curr->status = THREAD_READY;

	/* do_schedule 쓰지 않고 현재 스레드 상태를 바로 준비 상태로 변경  */
	// do_schedule (THREAD_READY);
	schedule ();
	intr_set_level (old_level);
}

bool compare (const struct list_elem *a, const struct list_elem *b, void *aux) 
{
	struct thread *thread_a = list_entry(a, struct thread, elem);
	struct thread *thread_b = list_entry(b, struct thread, elem);

	return thread_a->thread_tick < thread_b->thread_tick;
}


bool compare_priority (const struct list_elem *a, const struct list_elem *b, void *aux) 
{
	struct thread *thread_a = list_entry(a, struct thread, elem);
	struct thread *thread_b = list_entry(b, struct thread, elem);

	return thread_a->priority > thread_b->priority; 
	// 큰 숫자가 앞으로 오는 compare함수, ready_list는 우선순위가 높은 순서대로 정렬되어야 하기 때문에
}

void
thread_sleep() {
	/* 인터럽트 상태 저장을 위한 변수 */
	enum intr_level old_level;

	ASSERT (!intr_context ());

	/* 인터럽트 비활성화 */
	old_level = intr_disable ();

	/* aux: auxiliary data, 비교 함수에 넘겨주는 옵션 데이터 포인터 */
	/* -> 선택적 보조 인자, NULL 넣어주면 됨 */
	list_insert_ordered (&sleep_list, &thread_current()->elem, compare, NULL);
	thread_block ();

	/* 스케줄링으로 실행 재개 될 때 인터럽트 활성화 */
	intr_set_level (old_level);
}

void
thread_wakeUp(int64_t ticks)
{
	// timer_interrupt 안에서 호출되는 함수라 printf 빼는 게 좋음 
	
	while(!list_empty(&sleep_list))
	{
		// sleep_list에서 맨 앞 요소(스레드)를 확인
		struct thread *sleep_thread = list_entry(list_front(&sleep_list), struct thread, elem);
	
		// 맨 앞 스레드의 절대 시간 tick이 현재 tick 이하라면 깨워야 함 = pop
		if(sleep_thread->thread_tick <= ticks)
		{
			list_pop_front(&sleep_list);
			/* 이미 위에서 맨 앞 요소를 sleep_thread로 넣어줬기 때문에 조건 만족하면 바로 sleep_thread unblock */
			/* thread_unblock 안에서 sleep_thread를 ready_list 에 넣어줌 */
			thread_unblock(sleep_thread);
		}
		// 맨 앞 스레드의 절대 시간 tick이 현재 tick 이상이라면(깨울 때가 아니라면) 중단
		else
		{
			break;
		}
	}
}

/* Sets the current thread's priority to NEW_PRIORITY. */

// SONNY'S PART
// priority-change
// 우선순위가 변경되었을 때 sort해주기 list_front랑 priority 비교
void
thread_set_priority (int new_priority) {
	
	/* 현재 스레드의 priority를 변경 */
	struct thread *curr_t = thread_current ();
  	curr_t->priority = new_priority;
	
	/* ready 리스트 확인하기 전 intr 비활성화 */
	intr_disable ();
	
	if (!list_empty (&ready_list))
	{
		/* ready_list에 있는 첫 스레드 */
		struct thread *next_t = list_entry (list_front (&ready_list), struct thread, elem);
		if (curr_t->priority < next_t->priority)
		{
			/* priority 확인 후 intr 활성화 */
			intr_enable ();
			/* yield() 안에서 intr 비활성화, ready에 넣음, ready 상태로 변경, 스케줄 실행, intr 활성화 진행 */
			thread_yield ();
		}
	}
	else
	{
		intr_enable ();
	}
}

/* Returns the current thread's priority. */
int
thread_get_priority (void) {
	return thread_current ()->priority;
}

/* Sets the current thread's nice value to NICE. */
void
thread_set_nice (int nice UNUSED) {
	/* TODO: Your implementation goes here */
}

/* Returns the current thread's nice value. */
int
thread_get_nice (void) {
	/* TODO: Your implementation goes here */
	return 0;
}

/* Returns 100 times the system load average. */
int
thread_get_load_avg (void) {
	/* TODO: Your implementation goes here */
	return 0;
}

/* Returns 100 times the current thread's recent_cpu value. */
int
thread_get_recent_cpu (void) {
	/* TODO: Your implementation goes here */
	return 0;
}

/* Idle thread.  Executes when no other thread is ready to run.

   The idle thread is initially put on the ready list by
   thread_start().  It will be scheduled once initially, at which
   point it initializes idle_thread, "up"s the semaphore passed
   to it to enable thread_start() to continue, and immediately
   blocks.  After that, the idle thread never appears in the
   ready list.  It is returned by next_thread_to_run() as a
   special case when the ready list is empty. */
static void
idle (void *idle_started_ UNUSED) {
	struct semaphore *idle_started = idle_started_;

	idle_thread = thread_current ();
	sema_up (idle_started);

	for (;;) {
		/* Let someone else run. */
		intr_disable ();
		thread_block ();

		/* Re-enable interrupts and wait for the next one.

		   The `sti' instruction disables interrupts until the
		   completion of the next instruction, so these two
		   instructions are executed atomically.  This atomicity is
		   important; otherwise, an interrupt could be handled
		   between re-enabling interrupts and waiting for the next
		   one to occur, wasting as much as one clock tick worth of
		   time.

		   See [IA32-v2a] "HLT", [IA32-v2b] "STI", and [IA32-v3a]
		   7.11.1 "HLT Instruction". */
		asm volatile ("sti; hlt" : : : "memory");
	}
}

/* Function used as the basis for a kernel thread. */
static void
kernel_thread (thread_func *function, void *aux) {
	ASSERT (function != NULL);

	intr_enable ();       /* The scheduler runs with interrupts off. */
	function (aux);       /* Execute the thread function. */
	thread_exit ();       /* If function() returns, kill the thread. */
}


/* Does basic initialization of T as a blocked thread named
   NAME. */
static void
init_thread (struct thread *t, const char *name, int priority) {
	ASSERT (t != NULL);
	ASSERT (PRI_MIN <= priority && priority <= PRI_MAX);
	ASSERT (name != NULL);

	memset (t, 0, sizeof *t);
	t->status = THREAD_BLOCKED;
	strlcpy (t->name, name, sizeof t->name);
	t->tf.rsp = (uint64_t) t + PGSIZE - sizeof (void *);
	t->priority = priority;
	t->magic = THREAD_MAGIC;
}

/* Chooses and returns the next thread to be scheduled.  Should
   return a thread from the run queue, unless the run queue is
   empty.  (If the running thread can continue running, then it
   will be in the run queue.)  If the run queue is empty, return
   idle_thread. */
static struct thread *
next_thread_to_run (void) {
	if (list_empty (&ready_list))
		return idle_thread;
	else
		return list_entry (list_pop_front (&ready_list), struct thread, elem);
}

/* Use iretq to launch the thread */
void
do_iret (struct intr_frame *tf) {
	__asm __volatile(
			"movq %0, %%rsp\n"
			"movq 0(%%rsp),%%r15\n"
			"movq 8(%%rsp),%%r14\n"
			"movq 16(%%rsp),%%r13\n"
			"movq 24(%%rsp),%%r12\n"
			"movq 32(%%rsp),%%r11\n"
			"movq 40(%%rsp),%%r10\n"
			"movq 48(%%rsp),%%r9\n"
			"movq 56(%%rsp),%%r8\n"
			"movq 64(%%rsp),%%rsi\n"
			"movq 72(%%rsp),%%rdi\n"
			"movq 80(%%rsp),%%rbp\n"
			"movq 88(%%rsp),%%rdx\n"
			"movq 96(%%rsp),%%rcx\n"
			"movq 104(%%rsp),%%rbx\n"
			"movq 112(%%rsp),%%rax\n"
			"addq $120,%%rsp\n"
			"movw 8(%%rsp),%%ds\n"
			"movw (%%rsp),%%es\n"
			"addq $32, %%rsp\n"
			"iretq"
			: : "g" ((uint64_t) tf) : "memory");
}

/* Switching the thread by activating the new thread's page
   tables, and, if the previous thread is dying, destroying it.

   At this function's invocation, we just switched from thread
   PREV, the new thread is already running, and interrupts are
   still disabled.

   It's not safe to call printf() until the thread switch is
   complete.  In practice that means that printf()s should be
   added at the end of the function. */
static void
thread_launch (struct thread *th) {
	uint64_t tf_cur = (uint64_t) &running_thread ()->tf;
	uint64_t tf = (uint64_t) &th->tf;
	ASSERT (intr_get_level () == INTR_OFF);

	/* The main switching logic.
	 * We first restore the whole execution context into the intr_frame
	 * and then switching to the next thread by calling do_iret.
	 * Note that, we SHOULD NOT use any stack from here
	 * until switching is done. */
	__asm __volatile (
			/* Store registers that will be used. */
			"push %%rax\n"
			"push %%rbx\n"
			"push %%rcx\n"
			/* Fetch input once */
			"movq %0, %%rax\n"
			"movq %1, %%rcx\n"
			"movq %%r15, 0(%%rax)\n"
			"movq %%r14, 8(%%rax)\n"
			"movq %%r13, 16(%%rax)\n"
			"movq %%r12, 24(%%rax)\n"
			"movq %%r11, 32(%%rax)\n"
			"movq %%r10, 40(%%rax)\n"
			"movq %%r9, 48(%%rax)\n"
			"movq %%r8, 56(%%rax)\n"
			"movq %%rsi, 64(%%rax)\n"
			"movq %%rdi, 72(%%rax)\n"
			"movq %%rbp, 80(%%rax)\n"
			"movq %%rdx, 88(%%rax)\n"
			"pop %%rbx\n"              // Saved rcx
			"movq %%rbx, 96(%%rax)\n"
			"pop %%rbx\n"              // Saved rbx
			"movq %%rbx, 104(%%rax)\n"
			"pop %%rbx\n"              // Saved rax
			"movq %%rbx, 112(%%rax)\n"
			"addq $120, %%rax\n"
			"movw %%es, (%%rax)\n"
			"movw %%ds, 8(%%rax)\n"
			"addq $32, %%rax\n"
			"call __next\n"         // read the current rip.
			"__next:\n"
			"pop %%rbx\n"
			"addq $(out_iret -  __next), %%rbx\n"
			"movq %%rbx, 0(%%rax)\n" // rip
			"movw %%cs, 8(%%rax)\n"  // cs
			"pushfq\n"
			"popq %%rbx\n"
			"mov %%rbx, 16(%%rax)\n" // eflags
			"mov %%rsp, 24(%%rax)\n" // rsp
			"movw %%ss, 32(%%rax)\n"
			"mov %%rcx, %%rdi\n"
			"call do_iret\n"
			"out_iret:\n"
			: : "g"(tf_cur), "g" (tf) : "memory"
			);
}

/* Schedules a new process. At entry, interrupts must be off.
 * This function modify current thread's status to status and then
 * finds another thread to run and switches to it.
 * It's not safe to call printf() in the schedule(). */
/* 새로운 프로세스를 스케줄링합니다. 진입 시 인터럽트는 비활성화되어 있어야 합니다.
 * 이 함수는 현재 스레드의 상태를 status로 변경한 다음,
 * 실행할 다른 스레드를 찾아 해당 스레드로 전환합니다.
 * schedule() 내에서 printf()를 호출하는 것은 안전하지 않습니다. */
static void
do_schedule(int status) {
	ASSERT (intr_get_level () == INTR_OFF);
	ASSERT (thread_current()->status == THREAD_RUNNING);
	while (!list_empty (&destruction_req)) {
		struct thread *victim =
			list_entry (list_pop_front (&destruction_req), struct thread, elem);
		palloc_free_page(victim);
	}
	thread_current ()->status = status;
	schedule ();
}

static void
schedule (void) {
	struct thread *curr = running_thread ();
	struct thread *next = next_thread_to_run ();

	ASSERT (intr_get_level () == INTR_OFF);
	ASSERT (curr->status != THREAD_RUNNING);
	ASSERT (is_thread (next));
	/* Mark us as running. */
	next->status = THREAD_RUNNING;

	/* Start new time slice. */
	thread_ticks = 0;

#ifdef USERPROG
	/* Activate the new address space. */
	process_activate (next);
#endif

	if (curr != next) {
		/* If the thread we switched from is dying, destroy its struct
		   thread. This must happen late so that thread_exit() doesn't
		   pull out the rug under itself.
		   We just queuing the page free reqeust here because the page is
		   currently used by the stack.
		   The real destruction logic will be called at the beginning of the
		   schedule(). */
		if (curr && curr->status == THREAD_DYING && curr != initial_thread) {
			ASSERT (curr != next);
			list_push_back (&destruction_req, &curr->elem);
		}

		/* Before switching the thread, we first save the information
		 * of current running. */
		thread_launch (next);
	}
}

/* Returns a tid to use for a new thread. */
static tid_t
allocate_tid (void) {
	static tid_t next_tid = 1;
	tid_t tid;

	lock_acquire (&tid_lock);
	tid = next_tid++;
	lock_release (&tid_lock);

	return tid;
}
