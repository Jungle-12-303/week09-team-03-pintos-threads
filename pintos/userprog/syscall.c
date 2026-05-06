#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/loader.h"
#include "userprog/gdt.h"
#include "threads/flags.h"
#include "intrinsic.h"

/* NICK */
#include "filesys/filesys.h"
#include "threads/synch.h"
#include "threads/vaddr.h"
#include "threads/mmu.h"
/* NICK */

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

#define MSR_STAR 0xc0000081         /* Segment selector msr */
#define MSR_LSTAR 0xc0000082        /* Long mode SYSCALL target */
#define MSR_SYSCALL_MASK 0xc0000084 /* Mask for the eflags */

	/* NICK */

static struct lock filesys_lock;
	/* NICK */

void
syscall_init (void) {
	write_msr(MSR_STAR, ((uint64_t)SEL_UCSEG - 0x10) << 48  |
			((uint64_t)SEL_KCSEG) << 32);
	write_msr(MSR_LSTAR, (uint64_t) syscall_entry);

	/* The interrupt service rountine should not serve any interrupts
	 * until the syscall_entry swaps the userland stack to the kernel
	 * mode stack. Therefore, we masked the FLAG_FL. */
	write_msr(MSR_SYSCALL_MASK,
			FLAG_IF | FLAG_TF | FLAG_DF | FLAG_IOPL | FLAG_AC | FLAG_NT);
	
	/* NICK */
	lock_init(&filesys_lock);
	/* NICK */

}

static void
sys_exit (int status) {
	struct thread *curr = thread_current ();
	curr->exit_code = status;
	thread_exit ();
}

static void 
validate_user_addr (const void *uaddr) {
	struct thread *curr = thread_current ();

	if (uaddr == NULL ||
		!is_user_vaddr (uaddr) ||
		curr->pml4 == NULL ||
		pml4_get_page (curr->pml4, uaddr) == NULL) {
		sys_exit (-1);
	}
}

static void //1.유저가 넘긴 문자열이 유효한지 검증하는 함수
validate_user_string(const char *str)
{
	while(true)
	{
		validate_user_addr(str);

		if(*str ==  '\0') return;
		str++; 
	}
} 


static void //2. 버퍼가 유저 영역에 있는지 검증하는 함수
validate_user_buffer(const void *buffer, size_t size)
{
	const uint8_t *buf = buffer; 

	for(size_t i = 0; i < size; i++)
	{
		validate_user_addr(buf + i); //
	}
}

/* The main system call interface */
void
syscall_handler (struct intr_frame *f UNUSED) {
	// TODO: Your implementation goes here.
	// printf ("system call!\n");

	/* SONNY'S CODE */
	switch ((int)(f->R.rax)) //이곳에 write, exit, create , open ..
	{
	/* NICK */
	case SYS_WRITE:
	{
		int fd = (int) f -> R.rdi; 
		const char *buffer = (const char *)f -> R.rsi;
		size_t size = (size_t)f -> R.rdx;

		validate_user_buffer(buffer, size);

		if(fd == 1)
		{
			putbuf(buffer, size);
			f -> R.rax = size;
		}
		else
		{
			f -> R.rax = -1; 
		}
		return; 
	}

	case SYS_CREATE: 
	{
		// syscall2에서 커널이 아래처럼 정보를 가져옴
		const char *file = (const char * ) f -> R.rdi; 
		unsigned initial_size = (unsigned) f -> R.rsi;

		struct thread *curr = thread_current();
		validate_user_addr(file); 

		
/* 이제 null검사는 is_user_vaddr에서 하니까 필요없음		
		if(file == NULL) //create(NULL , 0); 
		{
			curr -> exit_code = -1; //현재 프로세스를 죽여
			thread_exit();
		//유효하지 않은 메모리 주소를 넘김 -> 프로그램 에러

		}
*/		

		if(file[0] == '\0') //create("", 0); 
		{
			f -> R.rax = false;
			return;
		//정상적인 주소를 줬지만, 내용이 비어있어 파일 생성 실패(false) 반환
		
		}
		
		//유저가 넘긴 주소 검증한다. 
		if(!is_user_vaddr(file))
		{
			curr -> exit_code = -1;
			thread_exit();
		}
		
		//lock 사용: 
		lock_acquire(&filesys_lock);
		bool ok = filesys_create(file, initial_size);
		lock_release(&filesys_lock);
		
		f-> R.rax = ok;
		return; 

	/* NICK */
	}

	case SYS_EXIT:
	/* KDA'S CODE - start */
		struct thread *curr = thread_current();
		
		curr->exit_code = f->R.rdi;
		/* KDA'S CODE - end */
		thread_exit ();
		break;
	
	default:
		break;
	}
	/* SONNY'S CODE */

}
