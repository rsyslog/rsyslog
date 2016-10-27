//#define _GNU_SOURCE
//#include <dlfcn.h>
#include <stdio.h>
 

int gethostname(char *name, size_t len)
{
	//*name++ = 'i';
	*name = '\0';
	return 0;
}
 
static void __attribute__((constructor))
my_init(void)
{
  printf("loaded\n");
  //orig_etry = dlsym(RTLD_NEXT, "original_entry_point");
}
