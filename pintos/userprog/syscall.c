#include "userprog/syscall.h"
#include "filesys/filesys.h"
#include "intrinsic.h"
#include "threads/flags.h"
#include "threads/interrupt.h"
#include "threads/loader.h"
#include "threads/synch.h"
#include "threads/thread.h"
#include "userprog/gdt.h"
#include <stdio.h>
#include <syscall-nr.h>

void syscall_entry (void);
void syscall_handler (struct intr_frame *);

/* KDA'S CODE */
static struct lock lock;
/* KDA'S CODE */

/* System call.
 *
 * Previously system call services was handled by the interrupt handler
 * (e.g. int 0x80 in linux). However, in x86-64, the manufacturer supplies
 * efficient path for requesting the system call, the `syscall` instruction.
 *
 * The syscall instruction works by reading the values from the the Model
 * Specific Register (MSR). For the details, see the manual. */

#define MSR_STAR         0xc0000081 /* Segment selector msr */
#define MSR_LSTAR        0xc0000082 /* Long mode SYSCALL target */
#define MSR_SYSCALL_MASK 0xc0000084 /* Mask for the eflags */

/* 구현 예정 목록
    fd_table_init
    file open 수정
    file close 수정
    free_fd_table
    alloc_fd_table
*/

/* SONNY'S CODE */

void
syscall_init (void) {
	write_msr (MSR_STAR, ((uint64_t) SEL_UCSEG - 0x10) << 48 |
	                             ((uint64_t) SEL_KCSEG) << 32);
	write_msr (MSR_LSTAR, (uint64_t) syscall_entry);

	/* The interrupt service rountine should not serve any interrupts
	 * until the syscall_entry swaps the userland stack to the kernel
	 * mode stack. Therefore, we masked the FLAG_FL. */
	write_msr (MSR_SYSCALL_MASK,
	           FLAG_IF | FLAG_TF | FLAG_DF | FLAG_IOPL | FLAG_AC | FLAG_NT);

	/* KDA'S CODE - start */
	lock_init (&lock);
	/* KDA'S CODE - end */
}

/* The main system call interface */
void
syscall_handler (struct intr_frame *f UNUSED) {
	// TODO: Your implementation goes here.

	/* KDA'S CODE - start*/
	struct thread *curr = thread_current ();
	/* KDA'S CODE - end */

	/* SONNY'S CODE */
	switch ((int) (f->R.rax)) {
	case SYS_EXIT:
		/* KDA'S CODE - start */
		curr->exit_code = f->R.rdi;
		/* KDA'S CODE - end */

		thread_exit ();
		break;

	/* KDA'S CODE - start */
	case SYS_CREATE:
		/* create는 성공/실패만 반환하면 됨 */

		/* bool create (const char *file, unsigned initial_size); */
		/* file = rdi, initial_size = rsi */
		/* rax = syscall 번호  */
		char *file = (char *) f->R.rdi;
		unsigned initial_size = (unsigned) f->R.rsi;

		/* 첫번째 인자값이 NULL일 때 종료 상태 코드 -1 반환 */
		/* PTE_ADDR(PTE): 페이지 테이블 엔트리(PTE) 값에서 "주소 부분만" 뽑아내는 매크로 -> 사용 X */
		/* pml4_get_page: 매핑된 커널 가상 주소 또는 NULL 반환 */
		/* todo: 현재는 문자열 전체가 아니라 시작 주소 한 점만 검증하는 로직, 추후 수정하면 좋을 듯 */
		if (file == NULL || file >= KERN_BASE || pml4_get_page (curr->pml4, (const void *) file) == NULL) {
			/* 종료 메세지 출력을 위해 exit_code -1로 설정 */
			curr->exit_code = -1;

			thread_exit ();
		} else {
			/* filesys_create 등 파일 시스템 관련 함수 호출 전 파일 시스템에 락을 걸어야 함 */
			/* Why?:
			/* 1. Pintos에서 제공하는 파일 시스템 코드엔 내부 동기화 기능이 구현되어 있지 않음 */
			/* 2. 여러 User Process가 동시에 시스템 콜을 호출하며는 것은 안전하지 않고, 서로 간섭을 일으키게 됨 */
			lock_acquire (&lock);

			f->R.rax = filesys_create (file, initial_size);

			/* 반환값을 rax로 넘기고 락 해제 */
			lock_release (&lock);
		}

		break;
		/* KDA'S CODE - end */

	case SYS_WRITE: /* write -SONNY- */
		// int fd = (int)(f->R.rdi);
		// void *buffer = (void *)(f->R.rsi);
		// unsigned size = (unsigned)(f->R.rdx);

		putbuf ((char *) (f->R.rsi), (size_t) (f->R.rdx));

		// if (fd = 0) {

		// } else if (fd = 1) {
		// 	putbuf((char *)(f->R.rsi), (size_t)(f->R.rdx));
		// }
		// else {
		// 	write(fd, buffer, size);
		// }

		break;

	case SYS_OPEN: /* SONNY'S CODE */
		// struct file *open_file = filesys_open(f->R.rdi);
		// if (open_file != NULL) {
		// 	curr->fd_table[curr->next_fd].file = open_file;
		// 	curr->next_fd++;
		// }
		break;
	default:
		break;
	}
	/* SONNY'S CODE */
}
