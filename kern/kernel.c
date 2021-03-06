/* EasiOS kernel.c
 * ----------------
 * Author(s): Daniel (Easimer) Meszaros
 * ----------------
 * Description: kernel entry point
 */

#include <kernel.h>
#include <stdio.h>
#include <multiboot2.h>
#include <dev/serial.h>
#include <string.h>
#include <dtables.h>
#include <stdlib.h>
#include <dev/timer.h>
#include <video.h>
#include <dev/kbd.h>
#include <mem.h>
#include <dev/mouse.h>
#include <text.h>
#include <krandom.h>
#include <time.h>
#include <amethyst.h>
#include <dev/ide.h>
#include <dev/pci.h>
#include <tar.h>
#include <dev/ethernet.h>
#include <kshell.h>
#include <users.h>
#include <stdarg.h>
#include <cpuid.h>
#include <dev/disk.h>
#include <net/slip.h>
#include <fs/fat32/fat_filelib.h>

struct cpu_desc cpu_desc;

const char* cmdline = NULL;
uint16_t __attribute__((aligned(4))) text_buffer[2000];
struct lua_apps lua_apps[LUA_APPS_N];

void kpanic(const char* msg, registers_t regs)
{
  asm volatile("cli");
  puts("-----------------\nEXCEPTION\n");
  puts(msg); putc('\n'); puts("-----------------\n");
  vsetcol(0x20, 0x67, 0xb2, 0xff);
  vcls();
  vsetcol(0xff, 0xff, 0xff, 0xff);
  vd_rectangle(FILL, 100, 100, 32, 32);
  vd_rectangle(FILL, 100, 200, 32, 32);
  vd_rectangle(FILL, 196, 100, 16, 132);
  struct {int x,y,w,h;} ayy[] = {{212, 100, 32, 16}, {244, 116, 32, 16},
  {276, 132, 16, 64},
  {212, 216, 32, 16}, {244, 200, 32, 16}};
  for(int i = 0; i < 5; i++)
    vd_rectangle(FILL, ayy[i].x, ayy[i].y, ayy[i].w, ayy[i].h);
  puts("drawn rectangle\n");
  vswap();
  puts("swapped\n");
  int64_t fx, fy;
  vd_print(100, 250, "Your PC ran into a problem and needs a restart.", &fx, &fy);
  vd_print(100, fy + 20, "We're just collecting some error info and then you can restart.", &fx, &fy);
  vswap();
  puts("swapped\n");
  int64_t px, py;
  vd_print(100, fy + 30, "If you'd like to know more, you can search online later for this error: ", &px, &py);
  vswap();
  puts("swapped\n");
  vd_print(100, py + 20, msg, &px, &py);
  vswap();
  void dumpdata(registers_t regs);
  dumpdata(regs);
  vd_print(100, py + 40, "Human readable debug data has been sent to the serial port.", NULL, NULL);
  vswap();
  hlt_loop:
  cpu_relax();
  goto hlt_loop;
}

void multiboot_enum(uint32_t mbp)
{
  struct multiboot_tag *tag;
  unsigned size;
  size = *(unsigned *)mbp;
  if(size == 0)
    return;
  char buffer[64];
  puts("Multiboot2 tags: ");
  itoa(size, buffer, 10);
  puts(buffer);
  putc('\n');
  for (tag = (struct multiboot_tag *) (mbp + 8);
  	 tag->type != MULTIBOOT_TAG_TYPE_END;
     tag = (struct multiboot_tag *) ((multiboot_uint8_t *) tag
        + ((tag->size + 7) & ~7)))
  {
    switch(tag->type)
    {
      case MULTIBOOT_TAG_TYPE_CMDLINE:
			{
				struct multiboot_tag_string *tagstr =
					(struct multiboot_tag_string *)tag;
				cmdline = (tagstr->string);
				if(strcmp(cmdline, "serial") == 0)
				{
					serinit();
          serinitport(COM1);
          puts("Serial enabled from cmdline!\n");
        }
				break;
			}
      case MULTIBOOT_TAG_TYPE_FRAMEBUFFER:
			{
				struct multiboot_tag_framebuffer_common* tagfb =
					(struct multiboot_tag_framebuffer_common*) tag;
				puts("Framebuffer");
				puts("\n  Address: ");
        itoa(tagfb->framebuffer_addr, buffer, 16);
        puts(buffer);
				puts(" Pitch: ");
        itoa(tagfb->framebuffer_pitch, buffer, 10);
        puts(buffer);
				puts(" Width: ");
        itoa(tagfb->framebuffer_width, buffer, 10);
        puts(buffer);
				puts(" Height: ");
        itoa(tagfb->framebuffer_height, buffer, 10);
        puts(buffer);
				puts(" Bits per pixel: ");
        itoa(tagfb->framebuffer_bpp, buffer, 10);
        puts(buffer);
        puts(" Type: ");
        itoa(tagfb->framebuffer_type, buffer, 10);
        puts(buffer);
				putc('\n');
        if(tagfb->framebuffer_type == 1)
        {
          vinit(tagfb->framebuffer_width, tagfb->framebuffer_height,
                tagfb->framebuffer_bpp, tagfb->framebuffer_pitch,
                tagfb->framebuffer_addr);
          tinit(80, 25, (uint16_t*)&text_buffer);
        }
        if(tagfb->framebuffer_type == 2)
        {
          tinit(tagfb->framebuffer_width, tagfb->framebuffer_height,
                (uint16_t*)(uint32_t)tagfb->framebuffer_addr);
        }
				break;
			}
      case MULTIBOOT_TAG_TYPE_MMAP:
			{
				struct multiboot_tag_mmap *tagmmap =
					(struct multiboot_tag_mmap *)tag;
				int mmap_n = (tagmmap->size - 16) / sizeof(struct multiboot_mmap_entry);
				memmgmt_init(tagmmap->entries, mmap_n);
        break;
			}
    }
  }
}

extern void sse_enable(void);

void cpu_check_features()
{
  puts("=============\n");
  unsigned int a = 0, b = 0, c = 0, d = 0;
  __get_cpuid (0, &a, &b, &c, &d);
  memcpy(&cpu_desc.vendor[0], &b, 4);
  memcpy(&cpu_desc.vendor[4], &d, 4);
  memcpy(&cpu_desc.vendor[8], &c, 4);
  printf("CPU Vendor: %s\n", cpu_desc.vendor);
  __get_cpuid (0x80000000, &a, &b, &c, &d);
  if(a >= 0x80000004)
  {
    for(unsigned int i = 0x80000002; i < 0x80000005; i++)
    {
      __get_cpuid(i, &a, &b, &c, &d);
      memcpy(&cpu_desc.brand[(i - 0x80000002) * 16], &a, 4);
      memcpy(&cpu_desc.brand[(i - 0x80000002) * 16 + 4], &b, 4);
      memcpy(&cpu_desc.brand[(i - 0x80000002) * 16 + 8], &c, 4);
      memcpy(&cpu_desc.brand[(i - 0x80000002) * 16 + 12], &d, 4);
    }
    printf("CPU Brand: %s\n", cpu_desc.brand);
  }
  puts("CPU Features:\n");
  __get_cpuid (1, &a, &b, &c, &d);
  cpu_desc.features1 = d;
  cpu_desc.features2 = c;
  #define CPUFEATREQ 1
  int cpu_required_features1[CPUFEATREQ] = {bit_SSE};
  for(int i = 0; i < CPUFEATREQ; i++)
  {
    if(!(cpu_desc.features1 & cpu_required_features1[i]))
    {
      registers_t regs;
      kpanic("CPU unsupported", regs);
    }
  }
  sse_enable();
  puts("\n=============\n");
}

struct cpu_desc* get_cpu_desc(void)
{
  return &cpu_desc;
}

void kmain(uint32_t magic, uint32_t mbp)
{
  memset(&lua_apps, 0, 16 * sizeof(struct lua_apps));
  disk_init();
  multiboot_enum(mbp);
  cpu_check_features();
  init_descriptor_tables();
  timerinit(1000);
  read_rtc();
  kbdinit();
  mouseinit();
  asm volatile("sti");
  while(time(NULL) == 0);
  pciinit();
  krandom_get();
  ethernet_list();
  printf("Welcome to %s!\n", KERNEL_NAME);
  puts("    (c) 2015-2016 Easimer Labs - All rights reserved.\n");
  size_t w = vgetw();
  size_t h = vgeth();
  if(w == 0 || h == 0)
  {
    puts("Cannot start window manager: no video. Falling back to kernel shell\n");
    kshell_main();
    puts("kshell returned\n"); return;
  }
  int ret = 0;
  do
  {
    //ret = eelphant_main(w, h);
    ret = amethyst_main(w, h);
  } while (ret);
  printf("Window manager returned with %d\n", ret);
  reboot("Window manager exited\n");
}

void reboot(const char* reason)
{
  uint8_t temp;
  puts("\n\n");
  puts("The system is going down for reboot NOW!\n");
  puts(reason); puts("\n\n\n");
  asm volatile("cli");
  do {
    temp = inb(0x64);
    if((temp & 1) != 0)
      inb(0x60);
  } while((temp & 2) != 0);
  outb(0x64, 0xFE);
  loop:
  asm volatile("hlt");
  goto loop;
}

void dumpdata(registers_t regs)
{
  puts("=================\nDump\n=================\n");
  printf("Registers:\nDS: 0x%x\nEDI: 0x%x ESI: 0x%x", regs.ds, regs.edi, regs.esi);
  printf("EBP: 0x%x ESP: 0x%x EBP: 0x%x\n", regs.ebp, regs.esp, regs.ebp);
  printf("EDX: 0x%x ECX: 0x%x EAX: 0x%x\n", regs.edx, regs.ecx, regs.eax);
  printf("EIP: 0x%x  CS: 0x%x EFLAGS: 0x%x\n", regs.eip, regs.cs, regs.eflags);
  printf("USERESP: 0x%x SS: 0x%x\n", regs.useresp, regs.ss);
  puts("=================\n");
  pci_ls();
  puts("=================\n");
  extern int64_t fbw, fbh, fbbpp, fbp;
  printf("Video\n\tWidth: %u Height: %u\n", fbw, fbh);
  printf("\tBPP: %u Pitch: %u\n", fbbpp, fbp);
  puts("=================\n");
}

void klog(const char* str)
{
  #if FAT32_DEBUG
  return;
  #endif
  FL_FILE* f = fl_fopen("/user/klog", "a");
  if(!f)
    return;
  fl_fwrite((uint8_t*)str, strlen(str), 1, f);
  fl_fclose(f);
}
