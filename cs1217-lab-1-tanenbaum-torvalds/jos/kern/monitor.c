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

#define CMDBUF_SIZE	80	// enough for one VGA text line


struct Command {
	const char *name;
	const char *desc;
	// return -1 to force monitor to exit
	int (*func)(int argc, char** argv, struct Trapframe* tf);
};

static struct Command commands[] = {
	{ "help", "Display this list of commands", mon_help },
	{ "kerninfo", "Display information about the kernel", mon_kerninfo },
	{ "colortest", "Test the color display", mon_coltest },
	{ "colorhelp", "Display information about how colors can be displayed", mon_colhelp },
	{ "backtrace", "Display a stack backtrace", mon_backtrace}
};

/***** Implementations of basic kernel monitor commands *****/

int
mon_help(int argc, char **argv, struct Trapframe *tf)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(commands); i++)
		cprintf("%s - %s\n", commands[i].name, commands[i].desc);
	return 0;
}

int
mon_kerninfo(int argc, char **argv, struct Trapframe *tf)
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

int
mon_coltest(int argc, char **argv, struct Trapframe *tf)
{
	cprintf("test 1 %d %x %s %c %o %x\n", 50, 0xab, "thinger", 'c', 32948, 12893);
	cprintf("color test \[2b}this should be cyan on green\[07} back to light gray on black\n");
	cprintf("malformed color statement \[%s}\n", "hi");
	cprintf("\[this is too long to be considered a colour and should be output as is}\n");
	cprintf("\[ag} the character set here is incorrect, the color will now change to green \[02}\n");
	cprintf("the colour should [%o %x] carry over for %d more line until \n", 1287, 312, 1);
	cprintf("it is \[07}reset\n");
	cprintf("long test with many \[%s\n\[0c}intertwined %d statements}\n\[07}, hopefully they are \[", "weirdly", 0);
	cprintf("interpreted %x \[a%x\[4e}} correctly\n\n", 32, 0xb);
	cprintf("\[07}color reset\n\n");

	cprintf("Testing all bg colors: \[07}0\[17}1\[27}2\[37}3\[47}4\[57}5\[67}6\[77}7\[87}8\[97}9\[a7}a\[b7}b\[c7}c\[d7}d\[e7}e\[f7}f\n");
	cprintf("Testing all fg colors: \[00}0\[01}1\[02}2\[03}3\[04}4\[05}5\[06}6\[07}7\[08}8\[09}9\[0a}a\[0b}b\[0c}c\[0d}d\[0e}e\[0f}f\n");
	cprintf("\[07}color reset\n\n");

	return 0;
}

int
mon_colhelp(int argc, char **argv, struct Trapframe *tf)
{
	cprintf("The display color can be set using the sequence: \\[xy}, where x = background colour and y = foreground color.\n");
	cprintf("x and y should belong to the character set [0-9, a-f]. The available colors are:");
	cprintf("0 = black\t 1 = blue\t 2 = green\t 3 = cyan\n");
	cprintf("4 = red\t 5 = purple\t 6 = orange\t 7 = light gray\n");
	cprintf("8 = dark gray\t 9 = light blue\t a = light green\t b = light cyan\n");
	cprintf("c = light red\t d = pink\t e = yellow\t f = white\n");
	return 0;
}

int
mon_backtrace(int argc, char **argv, struct Trapframe *tf)
{
	uint32_t *ebp = (uint32_t *) read_ebp();
	struct Eipdebuginfo sym;

	cprintf("Stack backtrace:\n");
	while (ebp != 0)
	{
		cprintf("  \[0c}ebp %08x  \[0b}eip %08x  \[4e}args %08x %08x %08x %08x %08x\[07}\n", ebp, ebp[1], ebp[2], ebp[3], ebp[4], ebp[5], ebp[6]);
		debuginfo_eip(ebp[1], &sym);

		cprintf("      \[f4}%s\[07}:\[0a}%d\[07}: \[0d}%.*s\[07}+\[0b}%d\[07}\n", sym.eip_file, sym.eip_line, sym.eip_fn_namelen, sym.eip_fn_name, ebp[1] - sym.eip_fn_addr);
		ebp = (uint32_t *) ebp[0];
	}

	return 0;
}

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
	while (1) {
		// gobble whitespace
		while (*buf && strchr(WHITESPACE, *buf))
			*buf++ = 0;
		if (*buf == 0)
			break;

		// save and scan past next arg
		if (argc == MAXARGS-1) {
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
	for (i = 0; i < ARRAY_SIZE(commands); i++) {
		if (strcmp(argv[0], commands[i].name) == 0)
			return commands[i].func(argc, argv, tf);
	}
	cprintf("Unknown command '%s'\n", argv[0]);
	return 0;
}

void
monitor(struct Trapframe *tf)
{
	char *buf;

	cprintf("Welcome to the JOS kernel monitor!\n");
	cprintf("Type 'help' for a list of commands.\n");


	while (1) {
		buf = readline("K> ");
		if (buf != NULL)
			if (runcmd(buf, tf) < 0)
				break;
	}
}
