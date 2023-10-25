// Simple command-line kernel monitor useful for
// controlling the kernel and exploring the system interactively.

#include <inc/stdio.h>
#include <inc/string.h>
#include <inc/memlayout.h>
#include <inc/assert.h>
#include <inc/x86.h>

#include <kern/console.h>
#include <kern/monitor.h>
#include <kern/kdebug.h>
#include <kern/pmap.h>

#define CMDBUF_SIZE 80 // enough for one VGA text line

struct Command
{
  const char *name;
  const char *desc;
  // return -1 to force monitor to exit
  int (*func)(int argc, char **argv, struct Trapframe *tf);
};

static struct Command commands[] = {
    {"help", "Display this list of commands", mon_help},
    {"kerninfo", "Display information about the kernel", mon_kerninfo},
    {"backtrace", "Display stack trace", mon_backtrace},
    {"showmappings", "Display physical page mappings for the given range of virtual addresses. Syntax: showmappings 0xstart 0xend",
      mon_showmappings},
    {"setperms", "Set or clear the permissions of the given page. Syntax: setperms s/c 0xvirtualaddr", mon_setperms},
    {"dumpcontents", "Display memory in any virtual/physical address range. Syntax dumpcontents v/p 0xstart 0xend", mon_dumpcontents}};

/***** Implementations of basic kernel monitor commands *****/

int mon_help(int argc, char **argv, struct Trapframe *tf)
{
  int i;

  for (i = 0; i < ARRAY_SIZE(commands); i++)
    cprintf("%s - %s\n", commands[i].name, commands[i].desc);
  return 0;
}

int mon_kerninfo(int argc, char **argv, struct Trapframe *tf)
{
  extern char _start[], entry[], etext[], edata[], end[];

  cprintf("Special kernel symbols:\n");
  cprintf("  _start                  %08x (phys)\n", _start);
  cprintf("  entry  %08x (virt)  %08x (phys)\n", entry, entry - KERNBASE);
  cprintf("  etext  %08x (virt)  %08x (phys)\n", etext, etext - KERNBASE);
  cprintf("  edata  %08x (virt)  %08x (phys)\n", edata, edata - KERNBASE);
  cprintf("  end    %08x (virt)  %08x (phys)\n", end, end - KERNBASE);
  cprintf("Kernel executable memory footprint: %dKB\n",
          ROUNDUP(end - entry, 1024) / 1024);
  return 0;
}

int mon_backtrace(int argc, char **argv, struct Trapframe *tf)
{
  uint32_t my_ebp;
  asm volatile("movl %%ebp,%0"
               : "=r"(my_ebp));
  cprintf("Stack backtrace:\n");
  uint32_t ebp = my_ebp;
  while (ebp != 0)
  {
    uint32_t eip = *((uint32_t *)ebp + 1);
    cprintf("ebp %08x eip %08x args", ebp, eip);
    for (int i = 2; i < 7; i++)
    {
      uint32_t arg = *((uint32_t *)ebp + i);
      cprintf(" %08x ", arg);
    }
    cprintf("\n");

    struct Eipdebuginfo info;
    debuginfo_eip(eip, &info);
    uintptr_t offset = eip - info.eip_fn_addr;
    cprintf("\t%s:%d: ", info.eip_file, info.eip_line);
    cprintf("%.*s+%d\n", info.eip_fn_namelen, info.eip_fn_name, offset);

    ebp = *(uint32_t *)ebp;
  }
  return 0;
}

static void
print_entry_flags(pte_t *entry)
{
  pte_t pte_copy = *entry;

  cprintf("[");
  for (int i = 0; i < 9; i++)
  {
    if (pte_copy & PTE_AVAIL)
    {
      cprintf("AV ");
      pte_copy &= ~PTE_AVAIL;
    }
    else if (pte_copy & PTE_G)
    {
      cprintf("G ");
      pte_copy &= ~PTE_G;
    }
    else if (pte_copy & PTE_PS)
    {
      cprintf("PS ");
      pte_copy &= ~PTE_PS;
    }
    else if (pte_copy & PTE_D)
    {
      cprintf("D ");
      pte_copy &= ~PTE_D;
    }
    else if (pte_copy & PTE_A)
    {
      cprintf("A ");
      pte_copy &= ~PTE_A;
    }
    else if (pte_copy & PTE_PCD)
    {
      cprintf("CD ");
      pte_copy &= ~PTE_PCD;
    }
    else if (pte_copy & PTE_PWT)
    {
      cprintf("WT ");
      pte_copy &= ~PTE_PWT;
    }
    else if (pte_copy & PTE_U)
    {
      cprintf("U ");
      pte_copy &= ~PTE_U;
    }
    else if (pte_copy & PTE_W)
    {
      cprintf("W ");
      pte_copy &= ~PTE_W;
    }
    else
    {
      cprintf("- ");
    }
  }
  cprintf((pte_copy & PTE_P) ? "P]" : "-]");
}

int mon_showmappings(int argc, char **argv, struct Trapframe *tf)
{
  pte_t *pte;
  pte_t pte_copy;
  physaddr_t page;

  int i;

  if (argc < 3) 
  {
    cprintf("invalid call, arguments not provided, check help\n");
    return 1;
  }
  
  if (argv[1][0] != '0' || argv[1][1] != 'x' || argv[2][0] != '0' || argv[2][1] != 'x')
  {
    cprintf("invalid call, arguments not formatted correctly, check help\n");
    return 1;
  }

  char *end_ptr = argv[1] + strlen(argv[1]) + 1;
  uintptr_t current_va = (uintptr_t)strtol(argv[1], &end_ptr, 16);

  end_ptr = argv[2] + strlen(argv[2]) + 1;
  uintptr_t end_vaddr = (uintptr_t)strtol(argv[2], &end_ptr, 16);

  pde_t *pgdir = (pde_t*) KADDR(rcr3());

  current_va = ROUNDDOWN(current_va, PGSIZE);
  end_vaddr = ROUNDUP(end_vaddr, PGSIZE);

  while (current_va <= end_vaddr)
  {
    pde_t *dentry = &pgdir[PDX(current_va)];
    pte = pgdir_walk(pgdir, (void *)current_va, 0);

    if (!pte || (*dentry & PTE_PS))
    {
      if (*dentry & PTE_PS)
      {
        cprintf("virtual [0x%08x] - mapped as super page to physical [0x%08x] - perm ", (dentry - pgdir) * SPGSIZE, PTE_ADDR(*dentry));
        print_entry_flags((pte_t*)dentry);
        cprintf("\n");

        current_va += SPGSIZE;
        continue;
      }
      else
      {
        cprintf("virtual [0x%08x] - not mapped\n", current_va);
      }
    }
    else if (!(*pte & PTE_P))
    {
      cprintf("virtual [0x%08x] - not present/unmapped\n", current_va);
    }
    else
    {
      cprintf("virtual [0x%08x] - physical [0x%08x] - perm ", current_va, PTE_ADDR(*pte));
      print_entry_flags(pte);
      cprintf("\n");
    }

    current_va += PGSIZE;
  }
  return 0;
}

int mon_setperms(int argc, char **argv, struct Trapframe *tf)
{
  pte_t *pte;
  pde_t *pde;

  pde_t *pgdir = KADDR(rcr3());

  if (argc < 3) 
  {
    cprintf("invalid call, arguments not provided, check help\n");
    return 1;
  }

  if (argv[2][0] != '0' || argv[2][1] != 'x' || !(argv[1][0] == 's' || argv[1][0] == 'c'))
  {
    cprintf("invalid call, arguments not formatted correctly, check help\n");
    return 1;
  }

  char *end_ptr = argv[2] + strlen(argv[2]) + 1;
  uintptr_t va = (uintptr_t)strtol(argv[2], &end_ptr, 16);

  va = ROUNDDOWN(va, PGSIZE);

  pde = &pgdir[PDX(va)];
  pte = pgdir_walk(pgdir, (void *) va, 0);

  int is_super_page = (*pde & PTE_PS);

  if (pte == NULL && !is_super_page)
  {
    cprintf("page is unmapped or not present\n");
  }

  char user = 0, writable = 0, present = 0;
  switch(argv[1][0])
  {

    case 's':
      while (user != '0' && user != '1')
      {
        cprintf("Enter user flag (1 = user-accessible, 0 = supervisor access only): ");
        user = getchar();
        cprintf("\n");
      }

      while (writable != '0' && writable != '1')
      {
        cprintf("Enter writable flag (1 = writable, 0 = read only): ");
        writable = getchar();
        cprintf("\n");
      }

      user = user - '0';
      writable = writable - '0';
      present = *pde & PTE_P;

      *pde >>= 3;
      *pde <<= 3;

      *pde = *pde | (PTE_U * user);
      *pde = *pde | (PTE_W * writable);
      *pde |= present;

      cprintf("set pde perms for [0x%08x]: perms ", va);
      print_entry_flags((pte_t *) pde);
      cprintf("\n");

      if (!is_super_page)
      {
        present = *pte & PTE_P;
        *pte >>= 3;
        *pte <<= 3;
        
        *pte = *pte | (PTE_U * user);
        *pte = *pte | (PTE_W * writable);
        *pte |= present;

        cprintf("set pte perms for [0x%08x]: perms ", va);
        print_entry_flags(pte);
        cprintf("\n");
      }
    break;

    case 'c':
      // Move the user and writable bits out, along with the present bit, and reset the present bit
      present = *pde & PTE_P;
      *pde >>= 3;
      *pde <<= 3;
      *pde |= present;

      cprintf("cleared pde for [0x%08x]: perms ", va);
      print_entry_flags((pte_t *) pde);
      cprintf("\n");

      if (!is_super_page)
      {
        present = *pte & PTE_P;
        *pte >>= 3;
        *pte <<= 3;
        *pte |= present;

        cprintf("cleared pte for [0x%08x]: perms ", va);
        print_entry_flags(pte);
        cprintf("\n");
      }
    break;
  }

  return 0;
}

void hexdump(void *buf, size_t len)
{
  // const char hex[] = "0123456789abcdef";
  uint32_t *p = (uint32_t *)buf;
  size_t i;

  for (i = 0; i < len/4; i += 1)
  {
    if (i % 8 == 0 && i >= 8) cprintf("\n");
    cprintf("0x%08x ", *(p + i));

    // for (size_t j = 0; j < 8; j++)
    // {
    //   uint8_t nibble = (p + i)[7 - j];
    //   cprintf("%c%c", hex[nibble >> 4], hex[nibble & 0x0f]);
    // }

    // cprintf(":");
    // for (size_t j = 0; j < 16; j++)
    // {
    //   if (i + j < len)
    //   {
    //     uint8_t byte = (p + i)[j];
    //     cprintf(" %c%c", hex[byte >> 4], hex[byte & 0x0f]);
    //   }
    //   else
    //   {
    //     cprintf("   ");
    //   }
    // }
    // cprintf("  ");

    // for (size_t j = 0; j < 16 && i + j < len; j++)
    // {
    //   uint8_t byte = (p + i)[j];
    //   cputchar(byte >= 32 && byte <= 126 ? byte : '.');
    // }
    // cputchar('\n');
  }

  if (len % 4 != 0)
  {
    cprintf("0x%08x", p + i);
  }

  cprintf("\n");
}

int mon_dumpcontents(int argc, char **argv, struct Trapframe *tf)
{
  if (argc < 4) 
  {
    cprintf("invalid call, arguments not provided, check help\n");
    return 1;
  }
  
  if (argv[2][0] != '0' || argv[2][1] != 'x' || argv[3][0] != '0' || argv[3][1] != 'x' || !(argv[1][0] == 'v' || argv[1][0] == 'p'))
  {
    cprintf("invalid call, arguments not formatted correctly, check help\n");
    return 1;
  }

  char physical = argv[1][0] == 'p';
  uintptr_t start_addr = strtol(argv[2], NULL, 0);
  uintptr_t end_addr = strtol(argv[3], NULL, 0);

  if (start_addr > end_addr)
  {
    cprintf("Start address must be less than or equal to end address\n");
    return 1;
  }

  if (physical)
  {
    if (end_addr >= npages * PGSIZE)
    {
      cprintf("Range exceeds physical memory bounds\n");
      return 1;
    }

    start_addr = (uintptr_t) KADDR(start_addr);
    end_addr = (uintptr_t) KADDR(end_addr);

    hexdump((void *) start_addr, end_addr - start_addr + 1);

    return 0;
  }

  pde_t *pgdir = KADDR(rcr3());
  
  uintptr_t test = ROUNDDOWN(start_addr, PGSIZE);
  uintptr_t end_test = ROUNDUP(end_addr, PGSIZE);

  while (test <= end_test)
  {
    pde_t *pde = &pgdir[PDX(test)];
    pte_t *pte = pgdir_walk(pgdir, (void *) test, 0);

    if (pte == NULL && !(*pde & PTE_PS))
    {
      cprintf("Range contains unmapped pages\n");
      return 1;
    }

    test += pte != NULL ? PGSIZE : SPGSIZE;
  }

  hexdump((void *) start_addr, end_addr - start_addr + 1);

  return 0;
}

// void
// display_pte(pte_t *pte_table, int offset)
//{
//	pte_t pte = pte_table[offset];
//	cprintf("%03x: %08x ",offset, PTE_ADDR(pte));
//	if ((pte & 0x1ff) == 0) {
//		cprintf("NONE\n");
//		return;
//	}
//	if (pte & PTE_P) cprintf("PTE_P ");
//	if (pte & PTE_W) cprintf("PTE_W ");
//	if (pte & PTE_U) cprintf("PTE_U ");
//
//	cprintf("\n");
// }
//
// int
// mon_dumpcontents(int argc, char **argv, struct Trapframe *tf)
//{
//	if (argc != 2) {
//		cprintf("usage: vaddr <virtual address>\n");
//		return 0;
//	}
//	uintptr_t address;
//	uintptr_t page = ROUNDDOWN(address, PGSIZE);
//	pde_t *pgdir = KADDR(rcr3());
//	cprintf("Page virtual address:\t\t%08x\n", page);
//	cprintf("Page dir virtual address:\t%08x\t", pgdir);
//	int pdoffset = PDX(address);
//	display_pte(pgdir, pdoffset);
//
//	pde_t pde = pgdir[pdoffset];
//	if (!(pde & PTE_P)) {
//		cprintf("Address not in page directory\n");
//		return 0;
//	}
//	pte_t *pagetable = KADDR(PTE_ADDR(pde));
//	cprintf("Page table virtual address:\t%08x\t", pagetable);
//	int ptoffset = PTX(address);
//	display_pte(pagetable, ptoffset);
//
//	pte_t pte = pagetable[ptoffset];
//	if (!(pte & PTE_P)) {
//                 cprintf("Address not in page table\n");
//                 return 0;
//         }
//	cprintf("Page frame address:\t\t%08x\n", PTE_ADDR(pte));
//	cprintf("Physical address:\t\t%08x\n", PTE_ADDR(pte) + PGOFF(address));
//	return 0;
// }

/***** Kernel monitor command interpreter *****/

#define WHITESPACE "\t\r\n "
#define MAXARGS 16

static int
runcmd(char *buf, struct Trapframe *tf)
{
  int argc;
  char *argv[MAXARGS];
  int i;

  // Parse the command buffer into whitespace-separated arguments
  argc = 0;
  argv[argc] = 0;
  while (1)
  {
    // gobble whitespace
    while (*buf && strchr(WHITESPACE, *buf))
      *buf++ = 0;
    if (*buf == 0)
      break;

    // save and scan past next arg
    if (argc == MAXARGS - 1)
    {
      cprintf("Too many arguments (max %d)\n", MAXARGS);
      return 0;
    }
    argv[argc++] = buf;
    while (*buf && !strchr(WHITESPACE, *buf))
      buf++;
  }
  argv[argc] = 0;

  // Lookup and invoke the command
  if (argc == 0)
    return 0;
  for (i = 0; i < ARRAY_SIZE(commands); i++)
  {
    if (strcmp(argv[0], commands[i].name) == 0)
      return commands[i].func(argc, argv, tf);
  }
  cprintf("Unknown command '%s'\n", argv[0]);
  return 0;
}

void monitor(struct Trapframe *tf)
{
  char *buf;

  cprintf("Welcome to the JOS kernel monitor!\n");
  cprintf("Type 'help' for a list of commands.\n");

  while (1)
  {
    buf = readline("K> ");
    if (buf != NULL)
      if (runcmd(buf, tf) < 0)
        break;
  }
}
