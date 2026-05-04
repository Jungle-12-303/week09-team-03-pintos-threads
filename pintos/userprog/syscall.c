#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/loader.h"
#include "userprog/gdt.h"
#include "threads/flags.h"
#include "intrinsic.h"

//NICK
#include <kernel/stdio.h>
#include "threads/vaddr.h"
//NICK

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
}

/* The main system call interface */
void
syscall_handler (struct intr_frame *f ) {
	
	//NICK - 자식 프로그램이 종료하려면 -> exit() system call를 kenel에 보낸다.
	// printf ("[trace] syscall no=%d, arg1=%d, arg3=%d\n",
	// 		(int) f->R.rax,
	// 		(int) f->R.rdi,
	// 		(int) f->R.rdx);

	//f는 syscall이 발생한 순간의 CPU 레지스터 상태를 담은 intr_frame 구조체 포인터다.
	//f->R.rax는 그 안에 저장된 rax 레지스터 값이다.
	
	switch(f -> R.rax){
		case SYS_WRITE: 
		{
			int fd = (int)f->R.rdi; //sys_write 번호== 10 이고 rdi에서 읽어와야함
			const char *buffer = (const char*)f->R.rsi;
			size_t size = (size_t)f->R.rdx;

			// == 와 = 의 차이 
			if(buffer == NULL || !is_user_vaddr(buffer))
			{
				//buffer 주소가 유저 영역 주소가 아니라면? -> 여기해석은 write() 호출 결과로 -1를 반환한다. 
				f->R.rax = -1;
				return;
			}
			if(fd == 1 )
			{
				putbuf(buffer, size);
				f->R.rax = size; 
			}else
			{
				f->R.rax = -1;
			}
			break;
		}
		case SYS_EXIT:
		{
			//int status = (int)f->R.rdi;
			//printf("%s: exit(%d)\n", thread_current() -> name, status);
			thread_exit();
			break;
		}

		default:
			
	// TODO: Your implementation goes here.
	//printf ("system call!\n");
		thread_exit ();
	}
	//NICk
}
