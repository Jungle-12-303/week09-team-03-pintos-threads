#include <console.h>
#include <stdarg.h>
#include <stdio.h>
#include "devices/serial.h"
#include "devices/vga.h"
#include "threads/init.h"
#include "threads/interrupt.h"
#include "threads/synch.h"

static void vprintf_helper (char, void *);
static void putchar_have_lock (uint8_t c);

/* The console lock.
   Both the vga and serial layers do their own locking, so it's
   safe to call them at any time.
   But this lock is useful to prevent simultaneous printf() calls
   from mixing their output, which looks confusing. */
/* 콘솔 락.
   VGA와 시리얼 계층은 각각 자체적으로 락을 수행하므로,
   언제든지 호출해도 안전하다.
   하지만 이 락은 동시에 실행되는 printf() 호출들의 출력이
   서로 섞여 혼란스럽게 보이는 것을 방지하는 데 유용하다. */
static struct lock console_lock;

/* True in ordinary circumstances: we want to use the console
   lock to avoid mixing output between threads, as explained
   above.

   False in early boot before the point that locks are functional
   or the console lock has been initialized, or after a kernel
   panics.  In the former case, taking the lock would cause an
   assertion failure, which in turn would cause a panic, turning
   it into the latter case.  In the latter case, if it is a buggy
   lock_acquire() implementation that caused the panic, we'll
   likely just recurse. */
/* 일반적인 상황에서는 true: 위에서 설명한 것처럼,
   스레드 간 출력이 섞이는 것을 피하기 위해 콘솔 락을 사용하고 싶다.

   락이 동작하기 전인 초기 부팅 단계이거나 콘솔 락이 초기화되기 전,
   또는 커널 패닉 이후에는 false이다. 전자의 경우 락을 잡으려고 하면
   assertion 실패가 발생하고, 이는 다시 패닉을 일으켜 후자의 경우가 된다.
   후자의 경우에는 패닉의 원인이 버그가 있는 lock_acquire() 구현이라면,
   아마 재귀적으로 같은 상황이 반복될 것이다. */
static bool use_console_lock;

/* It's possible, if you add enough debug output to Pintos, to
   try to recursively grab console_lock from a single thread.  As
   a real example, I added a printf() call to palloc_free().
   Here's a real backtrace that resulted:
/* Pintos에 디버그 출력을 충분히 많이 추가하다 보면,
   하나의 스레드에서 console_lock을 재귀적으로 잡으려고 시도할 수 있다.
   실제 예로, 나는 palloc_free()에 printf() 호출을 추가했다.
   그 결과 실제로 다음과 같은 백트레이스가 나왔다:
   lock_console()
   vprintf()
   printf()             - palloc() tries to grab the lock again
   palloc_free()        
   schedule_tail()      - another thread dying as we switch threads
   schedule()
   thread_yield()
   intr_handler()       - timer interrupt
   intr_set_level()
   serial_putc()
   putchar_have_lock()
   putbuf()
   sys_write()          - one process writing to the console
   syscall_handler()
   intr_handler()

   This kind of thing is very difficult to debug, so we avoid the
   problem by simulating a recursive lock with a depth
   counter.
   이런 종류의 문제는 디버깅하기 매우 어렵기 때문에,
   깊이 카운터를 사용해 재귀 락을 흉내 내는 방식으로
   문제를 피한다. */
static int console_lock_depth;

/* Number of characters written to console. */
static int64_t write_cnt;

/* Enable console locking. */
void
console_init (void) {
	lock_init (&console_lock);
	use_console_lock = true;
}

/* Notifies the console that a kernel panic is underway,
   which warns it to avoid trying to take the console lock from
   now on. */
void
console_panic (void) {
	use_console_lock = false;
}

/* Prints console statistics. */
void
console_print_stats (void) {
	printf ("Console: %lld characters output\n", write_cnt);
}

/* Acquires the console lock. */
	static void
acquire_console (void) {
	if (!intr_context () && use_console_lock) {
		if (lock_held_by_current_thread (&console_lock)) 
			console_lock_depth++; 
		else
			lock_acquire (&console_lock); 
	}
}

/* Releases the console lock. */
static void
release_console (void) {
	if (!intr_context () && use_console_lock) {
		if (console_lock_depth > 0)
			console_lock_depth--;
		else
			lock_release (&console_lock); 
	}
}

/* Returns true if the current thread has the console lock,
   false otherwise. */
static bool
console_locked_by_current_thread (void) {
	return (intr_context ()
			|| !use_console_lock
			|| lock_held_by_current_thread (&console_lock));
}

/* The standard vprintf() function,
   which is like printf() but uses a va_list.
   Writes its output to both vga display and serial port. */
/* 표준 vprintf() 함수.
   printf()와 비슷하지만 va_list를 사용한다.
   출력 내용을 VGA 디스플레이와 시리얼 포트 양쪽에 기록한다. */
int
vprintf (const char *format, va_list args) {
	int char_cnt = 0;

	acquire_console ();
	__vprintf (format, args, vprintf_helper, &char_cnt);
	release_console ();

	return char_cnt;
}

/* Writes string S to the console, followed by a new-line
   character. */
int
puts (const char *s) {
	acquire_console ();
	while (*s != '\0')
		putchar_have_lock (*s++);
	putchar_have_lock ('\n');
	release_console ();

	return 0;
}

/* Writes the N characters in BUFFER to the console. */
void
putbuf (const char *buffer, size_t n) {
	acquire_console ();
	while (n-- > 0)
		putchar_have_lock (*buffer++);
	release_console ();
}

/* Writes C to the vga display and serial port. */
int
putchar (int c) {
	acquire_console ();
	putchar_have_lock (c);
	release_console ();

	return c;
}

/* Helper function for vprintf(). */
static void
vprintf_helper (char c, void *char_cnt_) {
	int *char_cnt = char_cnt_;
	(*char_cnt)++;
	putchar_have_lock (c);
}

/* Writes C to the vga display and serial port.
   The caller has already acquired the console lock if
   appropriate. */
static void
putchar_have_lock (uint8_t c) {
	ASSERT (console_locked_by_current_thread ());
	write_cnt++;
	serial_putc (c);
	vga_putc (c);
}
