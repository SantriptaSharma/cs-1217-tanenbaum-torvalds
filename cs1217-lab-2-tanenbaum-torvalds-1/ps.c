#include "types.h"
#include "stat.h"
#include "pstat.h"
#include "param.h"
#include "user.h"

int
main(int argc, char *argv[])
{
  struct pstat p;
  int success = settickets(53);
  success = getpinfo(&p);

  if (success < 0) 
  {
    printf(1, "Unable to retrieve process information\n");
    exit();
  }

  printf(1, "ID\tTIX\tTCK\n");
  for (int i = 0; i < NPROC; i++)
  {
    if (!p.inuse[i]) continue;
    printf(1, "%d\t%d\t%d\n", p.pid[i], p.tickets[i], p.ticks[i]);
  }

  exit();
}
