#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/loader.h"
#include "userprog/gdt.h"
#include "threads/flags.h"
#include "intrinsic.h"
#include <string.h>
#include "threads/palloc.h"
#include "threads/vaddr.h"
#include "threads/mmu.h"
#include "userprog/process.h"

void syscall_entry (void);
void syscall_handler (struct intr_frame *);

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
}

/* The main system call interface */
void
syscall_handler (struct intr_frame *f UNUSED) {
	// TODO: Your implementation goes here.
	// printf ("system call!\n");

	/* SONNY'S CODE */
	switch ((int) (f->R.rax)) {
	case SYS_WRITE: /* write -SONNY- */
		putbuf ((char *) (f->R.rsi), (size_t) (f->R.rdx));
		break;

	case SYS_EXIT:
		/* KDA'S CODE - start */
		struct thread *curr = thread_current ();

		curr->exit_code = f->R.rdi;
		/* KDA'S CODE - end */
		thread_exit ();
		break;

	/* HOSEOK'S CODE start */
	case SYS_EXEC:
		char *user_cmd = (const char *) f->R.rdi;

		/* user_cmd가 NULL 인지, 실제 유저 영역인지, 유저주소가 pml4에 매핑되어있는지 */
		if (user_cmd == NULL ||
		    !is_user_vaddr (user_cmd) ||
		    !pml4_get_page (thread_current ()->pml4, user_cmd)) {
			f->R.rax = -1;
			return;
		}
		/* 커널공간 활당 process exec 할때 유저 메모리 지워버려서 커널공간에 할당해줘야함 */
		char *cmd_copy = palloc_get_page (0);

		/*R.rax */
		if (cmd_copy == NULL) {
			palloc_free_page (cmd_copy);
			f->R.rax = -1;
			return;
		}

		strlcpy (cmd_copy, user_cmd, PGSIZE);

		if (process_exec (cmd_copy) == -1) {
			f->R.rax = -1;
			thread_current ()->exit_code = -1;
			thread_exit ();
			break;
		}

	/* HOSEOK'S CODE end */
	default:
		break;
	}
	/* SONNY'S CODE */
}
