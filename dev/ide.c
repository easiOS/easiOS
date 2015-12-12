#include <dev/ide.h>
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <dtables.h>
#include <port.h>
#include <stdlib.h>
#include <string.h>
#include <dev/timer.h>

#define ATA_SR_BSY     0x80    // Busy
#define ATA_SR_DRDY    0x40    // Drive ready
#define ATA_SR_DF      0x20    // Drive write fault
#define ATA_SR_DSC     0x10    // Drive seek complete
#define ATA_SR_DRQ     0x08    // Data request ready
#define ATA_SR_CORR    0x04    // Corrected data
#define ATA_SR_IDX     0x02    // Inlex
#define ATA_SR_ERR     0x01    // Error

#define ATA_ER_BBK      0x80    // Bad sector
#define ATA_ER_UNC      0x40    // Uncorrectable data
#define ATA_ER_MC       0x20    // No media
#define ATA_ER_IDNF     0x10    // ID mark not found
#define ATA_ER_MCR      0x08    // No media
#define ATA_ER_ABRT     0x04    // Command aborted
#define ATA_ER_TK0NF    0x02    // Track 0 not found
#define ATA_ER_AMNF     0x01    // No address mark

#define ATA_CMD_READ_PIO          0x20
#define ATA_CMD_READ_PIO_EXT      0x24
#define ATA_CMD_READ_DMA          0xC8
#define ATA_CMD_READ_DMA_EXT      0x25
#define ATA_CMD_WRITE_PIO         0x30
#define ATA_CMD_WRITE_PIO_EXT     0x34
#define ATA_CMD_WRITE_DMA         0xCA
#define ATA_CMD_WRITE_DMA_EXT     0x35
#define ATA_CMD_CACHE_FLUSH       0xE7
#define ATA_CMD_CACHE_FLUSH_EXT   0xEA
#define ATA_CMD_PACKET            0xA0
#define ATA_CMD_IDENTIFY_PACKET   0xA1
#define ATA_CMD_IDENTIFY          0xEC

#define      ATAPI_CMD_READ       0xA8
#define      ATAPI_CMD_EJECT      0x1B

#define ATA_IDENT_DEVICETYPE   0
#define ATA_IDENT_CYLINDERS    2
#define ATA_IDENT_HEADS        6
#define ATA_IDENT_SECTORS      12
#define ATA_IDENT_SERIAL       20
#define ATA_IDENT_MODEL        54
#define ATA_IDENT_CAPABILITIES 98
#define ATA_IDENT_FIELDVALID   106
#define ATA_IDENT_MAX_LBA      120
#define ATA_IDENT_COMMANDSETS  164
#define ATA_IDENT_MAX_LBA_EXT  200

#define IDE_ATA        0x00
#define IDE_ATAPI      0x01

#define ATA_MASTER     0x00
#define ATA_SLAVE      0x01

#define ATA_REG_DATA       0x00
#define ATA_REG_ERROR      0x01
#define ATA_REG_FEATURES   0x01
#define ATA_REG_SECCOUNT0  0x02
#define ATA_REG_LBA0       0x03
#define ATA_REG_LBA1       0x04
#define ATA_REG_LBA2       0x05
#define ATA_REG_HDDEVSEL   0x06
#define ATA_REG_COMMAND    0x07
#define ATA_REG_STATUS     0x07
#define ATA_REG_SECCOUNT1  0x08
#define ATA_REG_LBA3       0x09
#define ATA_REG_LBA4       0x0A
#define ATA_REG_LBA5       0x0B
#define ATA_REG_CONTROL    0x0C
#define ATA_REG_ALTSTATUS  0x0C
#define ATA_REG_DEVADDRESS 0x0D

// Channels:
#define      ATA_PRIMARY      0x00
#define      ATA_SECONDARY    0x01

// Directions:
#define      ATA_READ      0x00
#define      ATA_WRITE     0x01

unsigned char ide_buf[2048] = {0};
unsigned static char ide_irq_invoked = 0;
unsigned static char atapi_packet[12] = {0xA8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

void idecallbackp(registers_t regs)
{
  puts("IDE IRQ primary\n");
}

void idecallbacks(registers_t regs)
{
  puts("IDE IRQ secondary\n");
}

unsigned char ide_read(unsigned char channel, unsigned char reg) {
   unsigned char result;
   if (reg > 0x07 && reg < 0x0C)
      ide_write(channel, ATA_REG_CONTROL, 0x80 | channels[channel].nIEN);
   if (reg < 0x08)
      result = inb(channels[channel].base + reg - 0x00);
   else if (reg < 0x0C)
      result = inb(channels[channel].base  + reg - 0x06);
   else if (reg < 0x0E)
      result = inb(channels[channel].ctrl  + reg - 0x0A);
   else if (reg < 0x16)
      result = inb(channels[channel].bmide + reg - 0x0E);
   if (reg > 0x07 && reg < 0x0C)
      ide_write(channel, ATA_REG_CONTROL, channels[channel].nIEN);
   return result;
}

void ide_write(unsigned char channel, unsigned char reg, unsigned char data) {
   if (reg > 0x07 && reg < 0x0C)
      ide_write(channel, ATA_REG_CONTROL, 0x80 | channels[channel].nIEN);
   if (reg < 0x08)
      outb(channels[channel].base  + reg - 0x00, data);
   else if (reg < 0x0C)
      outb(channels[channel].base  + reg - 0x06, data);
   else if (reg < 0x0E)
      outb(channels[channel].ctrl  + reg - 0x0A, data);
   else if (reg < 0x16)
      outb(channels[channel].bmide + reg - 0x0E, data);
   if (reg > 0x07 && reg < 0x0C)
      ide_write(channel, ATA_REG_CONTROL, channels[channel].nIEN);
}

void ide_read_buffer(unsigned char channel, unsigned char reg, unsigned int buffer,
                     unsigned int quads) {
   /* WARNING: This code contains a serious bug. The inline assembly trashes ES and
    *           ESP for all of the code the compiler generates between the inline
    *           assembly blocks.
    */
   if (reg > 0x07 && reg < 0x0C)
      ide_write(channel, ATA_REG_CONTROL, 0x80 | channels[channel].nIEN);
   asm("pushw %es; movw %ds, %ax; movw %ax, %es");
   if (reg < 0x08)
      insl(channels[channel].base  + reg - 0x00, buffer, quads);
   else if (reg < 0x0C)
      insl(channels[channel].base  + reg - 0x06, buffer, quads);
   else if (reg < 0x0E)
      insl(channels[channel].ctrl  + reg - 0x0A, buffer, quads);
   else if (reg < 0x16)
      insl(channels[channel].bmide + reg - 0x0E, buffer, quads);
   asm("popw %es;");
   if (reg > 0x07 && reg < 0x0C)
      ide_write(channel, ATA_REG_CONTROL, channels[channel].nIEN);
}

unsigned char ide_polling(unsigned char channel, unsigned int advanced_check) {

   // (I) Delay 400 nanosecond for BSY to be set:
   // -------------------------------------------------
   for(int i = 0; i < 4; i++)
      ide_read(channel, ATA_REG_ALTSTATUS); // Reading the Alternate Status port wastes 100ns; loop four times.

   // (II) Wait for BSY to be cleared:
   // -------------------------------------------------
   while (ide_read(channel, ATA_REG_STATUS) & ATA_SR_BSY)
      ; // Wait for BSY to be zero.

   if (advanced_check) {
      unsigned char state = ide_read(channel, ATA_REG_STATUS); // Read Status Register.

      // (III) Check For Errors:
      // -------------------------------------------------
      if (state & ATA_SR_ERR)
         return 2; // Error.

      // (IV) Check If Device fault:
      // -------------------------------------------------
      if (state & ATA_SR_DF)
         return 1; // Device Fault.

      // (V) Check DRQ:
      // -------------------------------------------------
      // BSY = 0; DF = 0; ERR = 0 so we should check for DRQ now.
      if ((state & ATA_SR_DRQ) == 0)
         return 3; // DRQ should be set

   }

   return 0; // No Error.

}

unsigned char ide_print_error(unsigned int drive, unsigned char err) {
   if (err == 0)
      return err;

   puts("IDE:");
   if (err == 1) {puts("- Device Fault\n     "); err = 19;}
   else if (err == 2) {
      unsigned char st = ide_read(ide_devices[drive].Channel, ATA_REG_ERROR);
      if (st & ATA_ER_AMNF)   {puts("- No Address Mark Found\n     ");   err = 7;}
      if (st & ATA_ER_TK0NF)   {puts("- No Media or Media Error\n     ");   err = 3;}
      if (st & ATA_ER_ABRT)   {puts("- Command Aborted\n     ");      err = 20;}
      if (st & ATA_ER_MCR)   {puts("- No Media or Media Error\n     ");   err = 3;}
      if (st & ATA_ER_IDNF)   {puts("- ID mark not Found\n     ");      err = 21;}
      if (st & ATA_ER_MC)   {puts("- No Media or Media Error\n     ");   err = 3;}
      if (st & ATA_ER_UNC)   {puts("- Uncorrectable Data Error\n     ");   err = 22;}
      if (st & ATA_ER_BBK)   {puts("- Bad Sectors\n     ");       err = 13;}
   } else  if (err == 3)           {puts("- Reads Nothing\n     "); err = 23;}
     else  if (err == 4)  {puts("- Write Protected\n     "); err = 8;}
   /*puts("- [%s %s] %s\n",
      (const char *[]){"Primary", "Secondary"}[ide_devices[drive].Channel], // Use the channel as an index into the array
      (const char *[]){"Master", "Slave"}[ide_devices[drive].Drive], // Same as above, using the drive
      ide_devices[drive].Model);*/

   return err;
}

void ide_initialize(unsigned int BAR0, unsigned int BAR1, unsigned int BAR2, unsigned int BAR3,
unsigned int BAR4) {

   int i, j, k, count = 0;
   puts("detecting io ports...");
   // 1- Detect I/O Ports which interface IDE Controller:
   channels[ATA_PRIMARY  ].base  = (BAR0 & 0xFFFFFFFC) + 0x1F0 * (!BAR0);
   channels[ATA_PRIMARY  ].ctrl  = (BAR1 & 0xFFFFFFFC) + 0x3F6 * (!BAR1);
   channels[ATA_SECONDARY].base  = (BAR2 & 0xFFFFFFFC) + 0x170 * (!BAR2);
   channels[ATA_SECONDARY].ctrl  = (BAR3 & 0xFFFFFFFC) + 0x376 * (!BAR3);
   channels[ATA_PRIMARY  ].bmide = (BAR4 & 0xFFFFFFFC) + 0; // Bus Master IDE
   channels[ATA_SECONDARY].bmide = (BAR4 & 0xFFFFFFFC) + 8; // Bus Master IDE
   // 2- Disable IRQs:
   puts("disabling IRQs...");
   ide_write(ATA_PRIMARY  , ATA_REG_CONTROL, 2);
   ide_write(ATA_SECONDARY, ATA_REG_CONTROL, 2);
   // 3- Detect ATA-ATAPI Devices:
   puts("detecting ata-atapi devices...");
for (i = 0; i < 2; i++)
   for (j = 0; j < 2; j++) {

      unsigned char err = 0, type = IDE_ATA, status;
      ide_devices[count].Reserved = 0; // Assuming that no drive here.

      puts("selecting drive...");
      // (I) Select Drive:
      ide_write(i, ATA_REG_HDDEVSEL, 0xA0 | (j << 4)); // Select Drive.
      //sleep 1
      puts("sending ata identify cmd...");
      // (II) Send ATA Identify Command:
      ide_write(i, ATA_REG_COMMAND, ATA_CMD_IDENTIFY);
      //sleep 1
      puts("detecting device...");
      // (III) Polling:
      if (ide_read(i, ATA_REG_STATUS) == 0) continue; // If Status = 0, No Device.

      while(1) {
        puts("reading status...");
         status = ide_read(i, ATA_REG_STATUS);
         if ((status & ATA_SR_ERR)) {err = 1; break;} // If Err, Device is not ATA.
         if (!(status & ATA_SR_BSY) && (status & ATA_SR_DRQ)) break; // Everything is right.
         char b[16];
         puts(itoa(status, b, 16));
         putc('\n');
      }

      // (IV) Probe for ATAPI Devices:

      if (err != 0) {
         unsigned char cl = ide_read(i, ATA_REG_LBA1);
         unsigned char ch = ide_read(i, ATA_REG_LBA2);

         if (cl == 0x14 && ch ==0xEB)
            type = IDE_ATAPI;
         else if (cl == 0x69 && ch == 0x96)
            type = IDE_ATAPI;
         else
            continue; // Unknown Type (may not be a device).

         ide_write(i, ATA_REG_COMMAND, ATA_CMD_IDENTIFY_PACKET);
         sleep(1);
      }

      // (V) Read Identification Space of the Device:
      ide_read_buffer(i, ATA_REG_DATA, (unsigned int) ide_buf, 128);

      // (VI) Read Device Parameters:
      ide_devices[count].Reserved     = 1;
      ide_devices[count].Type         = type;
      ide_devices[count].Channel      = i;
      ide_devices[count].Drive        = j;
      ide_devices[count].Signature    = *((unsigned short *)(ide_buf + ATA_IDENT_DEVICETYPE));
      ide_devices[count].Capabilities = *((unsigned short *)(ide_buf + ATA_IDENT_CAPABILITIES));
      ide_devices[count].CommandSets  = *((unsigned int *)(ide_buf + ATA_IDENT_COMMANDSETS));

      // (VII) Get Size:
      if (ide_devices[count].CommandSets & (1 << 26))
         // Device uses 48-Bit Addressing:
         ide_devices[count].Size   = *((unsigned int *)(ide_buf + ATA_IDENT_MAX_LBA_EXT));
      else
         // Device uses CHS or 28-bit Addressing:
         ide_devices[count].Size   = *((unsigned int *)(ide_buf + ATA_IDENT_MAX_LBA));

      // (VIII) String indicates model of device (like Western Digital HDD and SONY DVD-RW...):
      for(k = 0; k < 40; k += 2) {
         ide_devices[count].Model[k] = ide_buf[ATA_IDENT_MODEL + k + 1];
         ide_devices[count].Model[k + 1] = ide_buf[ATA_IDENT_MODEL + k];}
      ide_devices[count].Model[40] = 0; // Terminate String.

      count++;
   }
   /*
// 4- Print Summary:
for (i = 0; i < 4; i++)
   if (ide_devices[i].Reserved == 1) {
      puts("Found "); puts((const char *[]){"ATA", "ATAPI"}[ide_devices[i].Type]);
      puts(" Drive ");
      char b[64];
      puts(itoa(ide_devices[i].Size / 1024 / 1024 / 2, b, 10)); puts(" GB - ");
      //puts(ide_devices[i].Model);
      puts("\n");
   }*/
}

void ideinit()
{
  puts("IDE init...\n");
  //register callback
  //register_interrupt_handler(IRQ14, idecallbackp);
  //register_interrupt_handler(IRQ15, idecallbacks);
  ide_initialize(0x1F0, 0x3F6, 0x170, 0x376, 0x000);
  puts("done!\n");
}