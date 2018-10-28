/*
 *  MakeArray.c - Creates a array of Char * out of a given Textline
 *
 *  Original by Spy/tRSi - Rewritten and bugfixed by SieGeL/tRSi
 */
#include <stdlib.h>
#include <strings.h>
#include <exec/memory.h>
#include <proto/exec.h>
#include <proto/fame.h>
#include <libraries/fame.h>
#include <fame/fame.h>
#include "struct_ex.h"
#include "proto.h"

struct StringList
	{
  char *String ;
  struct StringList *next ;
	}*sl1,*sl2 ;

static void Free_Structs(void);

static BOOL INSERT (char *s)
	{
  if (!sl1)
  	{
		if(!(sl1 = (struct StringList *) AllocPooled (mem_pool,sizeof (struct StringList)))) return(FALSE);
    sl2 = sl1;
  	}
  else
  	{
		if(!(sl2->next = (struct StringList *) AllocPooled (mem_pool,sizeof (struct StringList)))) return(FALSE);
    sl2 = sl2->next;
  	}
  sl2->next = NULL;
  if(!(sl2->String = (char *) AllocPooled (mem_pool,(strlen(s)+2)))) return(FALSE);
  strcpy (sl2->String,s);
	return(TRUE);
	}

static void MoveStrings (char **res,struct StringList *sl)
	{
	struct 	StringList *slb;
	char 		**strs;

  slb = sl;
  strs = res;
  while(slb)
  	{
    *strs = slb->String;
    strs++;
    slb = slb->next;
  	}
	}

char **MakeArray (char *s,char sep)
	{
	char 	*str1,*str2,**result;
	int 	i;

  sl1 = NULL;
	sl2 = NULL;
  str1 = s;
  i = 0;
	while((str2 = FAMEStrChr(str1,sep)))
  	{
    *str2 = 0;
    if(INSERT (str1)==FALSE) return(NULL);
    i++;
    *str2 = ' ';
    while ((*str2 != 0) && (*str2 == sep))
    str2++;
    str1 = str2;
  	}
  if (*str1 != 0)
  	{
		if(INSERT (str1)==FALSE) return(NULL);
		}
  i++;
  if(!(result = (char **) AllocPooled (mem_pool,(i+1)*4))) return(NULL);
  MoveStrings (result,sl1);
  return (result);
	}

void FreeArray (char **strings)
	{
	char 	**strs;
	int 	i;

	i = 0 ;
  strs = strings;
  while (*strs)
  	{
    i++ ;
    FreePooled (mem_pool,*strs,sizeof(*strs));
    strs++ ;
  	}
  i++ ;
	FreePooled(mem_pool,strings,sizeof(strings)*(i+1));
	Free_Structs();
	}

static void Free_Structs(void)
	{
	struct StringList *h;
	while(sl1)
		{
		h=sl1;
		sl1=sl1->next;
		FreePooled(mem_pool,h,sizeof(struct StringList));
		}
	sl1=NULL;sl2=NULL;
	}
