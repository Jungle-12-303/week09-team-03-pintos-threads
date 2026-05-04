#ifndef THREADS_INTERRUPT_H
#define THREADS_INTERRUPT_H

#include <stdbool.h>
#include <stdint.h>

/* Interrupts on or off? */
enum intr_level {
	INTR_OFF,             /* Interrupts disabled. */
	INTR_ON               /* Interrupts enabled. */
};

enum intr_level intr_get_level (void);
enum intr_level intr_set_level (enum intr_level);
enum intr_level intr_enable (void);
enum intr_level intr_disable (void);

/* Interrupt stack frame. */
struct gp_registers {
	uint64_t r15;	/* 일반 목적 레지스터, 함수 호출 후에도 보존되는 -SONNY- */
	uint64_t r14;	/* 일반 목적 레지스터, 함수 호출 후에도 보존되는 -SONNY- */
	uint64_t r13;	/* 일반 목적 레지스터, 함수 호출 후에도 보존되는 -SONNY- */
	uint64_t r12;	/* 일반 목적 레지스터, 함수 호출 후에도 보존되는 -SONNY- */
	uint64_t r11;	/* syscall 때 플래그 저장에 사용됨 -SONNY- */
	uint64_t r10;	/* 시스템 콜의 4번째 인자 -SONNY- */
	uint64_t r9;	/* 함수/시스템 콜의 6번째 인자 -SONNY- */
	uint64_t r8;	/* 함수/시스템 콜의 5번째 인자 -SONNY- */
	uint64_t rsi;	/* 함수/시스템 콜의 2번째 인자 -SONNY- */
	uint64_t rdi;	/* 함수/시스템 콜의 1번째 인자 -SONNY- */
	uint64_t rbp;	/* 스택 프레임 기준 포인터 -SONNY- */
	uint64_t rdx;	/* 일반 목적 레지스터, 함수/시스템 콜의 3번째 인자 -SONNY- */
	uint64_t rcx;	/* 일반 목적 레지스터, syscall 복귀 주소와도 연관 있음 -SONNY- */
	uint64_t rbx;	/* 일반 목적 레지스터, 함수 호출 후에도 보존되는 편? -SONNY- */
	uint64_t rax;	/* 반환값, 시스템 콜 번호 저장 -SONNY- */
} __attribute__((packed));

struct intr_frame {
	/* Pushed by intr_entry in intr-stubs.S.
	   These are the interrupted task's saved registers. */
		 /* intr-stubs.S의 intr_entry에 의해 스택에 push된다.
   이것들은 인터럽트된 작업의 저장된 레지스터들이다. */
	struct gp_registers R;
	uint16_t es;
	uint16_t __pad1;
	uint32_t __pad2;
	uint16_t ds;
	uint16_t __pad3;
	uint32_t __pad4;
	/* Pushed by intrNN_stub in intr-stubs.S. */
	uint64_t vec_no; /* Interrupt vector number. */
/* Sometimes pushed by the CPU,
   otherwise for consistency pushed as 0 by intrNN_stub.
   The CPU puts it just under `eip', but we move it here. */
	uint64_t error_code;
/* Pushed by the CPU.
   These are the interrupted task's saved registers. */

/* uintptr_r = 포인터 주소값을 안전하게 담을 수 있는 unsigned integer 타입 */
	uintptr_t rip;
	uint16_t cs;
	uint16_t __pad5;
	uint32_t __pad6;
	uint64_t eflags;
	uintptr_t rsp;
	uint16_t ss;
	uint16_t __pad7;
	uint32_t __pad8;
} __attribute__((packed));

typedef void intr_handler_func (struct intr_frame *);

void intr_init (void);
void intr_register_ext (uint8_t vec, intr_handler_func *, const char *name);
void intr_register_int (uint8_t vec, int dpl, enum intr_level,
                        intr_handler_func *, const char *name);
bool intr_context (void);
void intr_yield_on_return (void);

void intr_dump_frame (const struct intr_frame *);
const char *intr_name (uint8_t vec);

#endif /* threads/interrupt.h */
