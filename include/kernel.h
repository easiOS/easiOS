#ifndef H_KERNEL
#define H_KERNEL

#include <stdint.h>
#include <dtables.h>

#define KERNEL_NAME "easiOS v0.3.12"

struct cpu_desc
{
  char vendor[16];
  char brand[48];
  uint32_t features1;
  uint32_t features2;
} __attribute__((packed, aligned(4)));

struct lua_apps
{
	char name[32];
	void* address;
} __attribute__((packed, aligned(4)));

void kmain(uint32_t magic, uint32_t mbp);
void kpanic(const char* msg, registers_t regs);
void reboot(const char* reason);
void kernupd_init(void);
struct cpu_desc* get_cpu_desc(void);
#define LUA_APPS_N 16

static inline void cpu_relax(void)
{
	asm volatile("rep; nop" ::: "memory");
}

#endif
