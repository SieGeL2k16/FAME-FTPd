/****************************************************************************************
 *  PROJECT: FAME-FTPd / FTPDoor
 *     FILE: init.c
 *  PURPOSE: FTP Upload Manager for FAME-FTPd. Handles the complete upload process
 *           including movement of files, byte accounting etc.
 *  CREATED: 20-JAN-2004
 * MODIFIED: 20-JAN-2004
 *   AUTHOR: Sascha 'SieGeL' Pfalz
 ****************************************************************************************/

#include <exec/exec.h>
#include <exec/types.h>
#include <exec/nodes.h>
#include <exec/lists.h>
#include <Fame/fameDoorProto.h>
#include <pragmas/utility_pragmas.h>
#include <pragmas/date_pragmas.h>
#include <proto/locale.h>
#include <proto/date.h>
#include <libraries/date.h>
#include <utility/date.h>
#include <utility/tagitem.h>
#include "version.h"
#include "/struct_ex.h"
#include "/global_defines.h"

/****************************************************************************
 * Prototypes
 ****************************************************************************/

void FTPInit(void);
void free_elements (struct Liste *l);

extern void wb(char*);

/****************************************************************************
 * Global variables
 ****************************************************************************/

struct Liste
	{
	struct FileInfoBlock fib;	// FIB struct of current element
	struct Liste *next;       // Next-Pointer for following elements
	}*Fib,*Fib2;

struct FileInfoBlock *myfib;
static char NO_MEM[] = "[37mUnable to allocate memory!!![m";

/****************************************************************************
 *  FUNCTION: FTPInit()
 *   PURPOSE: Init check function. Tests if they are any files for the
 *            current user, if files are found they are uploaded in some
 *            kind of simulation mode.
 * PARAMETER: none
 *   RETURNS: none
 * ADD. INFO: All files are stored in a directory configured via the config
 *            tool. Every user gets it's own directory inside the master dir
 *            (usernumber is the dirname). All files inside this folder are
 *            treated as files to be uploaded.
 ****************************************************************************/

void FTPInit(void)
	{
	static	char ftpdir[] 	= "FAME:FTPData/";
	static	char pattern[]	= "#?";
  char    dirbuffer[200],buf[200], patbuf[255];
	long		unum,myfibsize;
	BPTR    FLock;

	PutStringFormat("\n\r[35mFTPInit [mv%s [34m- [36mChecking for new files[34m...[m",COMPILE_VERSION);
	GetCommand(buf,0,0,0,NR_SlotNumber);
	unum = MyFAMEDoorMsg->fdom_Data2;
  FAMEStrCopy(ftpdir,dirbuffer,199);
  SPrintf(buf,"%ld/",unum);
	AddPart(dirbuffer,buf,199);
  myfibsize=sizeof(struct FileInfoBlock);
	if(ParsePatternNoCase(pattern,patbuf,255)==-1)
		{
    wb("\n\r[37mUnable to parse pattern for directory reading![m\n\n\r");
		}
	if(FLock=Lock(dirbuffer,ACCESS_READ))
		{
		Fib=NULL;Fib2=NULL;
		if(myfib=AllocPooled(mem_pool,myfibsize))
			{
			if(Examine(FLock,myfib))
				{
				if(myfib->fib_DirEntryType>=0)
					{
					while(ExNext(FLock,myfib))
						{
						if(!MatchPatternNoCase(patbuf,myfib->fib_FileName)) continue;
						if(!Fib)
							{
							if(!(Fib=(struct Liste *)AllocPooled(mem_pool,sizeof(struct Liste))))
								{
								UnLock(FLock);
                wb(NO_MEM);
                }
							Fib2=Fib;
							}
						else
							{
							if(!(Fib2->next=(struct Liste *)AllocPooled(mem_pool,sizeof(struct Liste))))
								{
								UnLock(FLock);
                wb(NO_MEM);
                }
							Fib2=Fib2->next;
							}
						Fib2->next=NULL;
						CopyMem((APTR) myfib,(APTR) Fib2,sizeof(struct FileInfoBlock));
						}
					}
				else
					{
					if(MatchPatternNoCase(patbuf,myfib->fib_FileName))
						{
	          if(!(Fib=(struct Liste *)AllocPooled(mem_pool,sizeof(struct Liste))))
							{
							UnLock(FLock);
							wb(NO_MEM);
							}
						Fib->next=NULL;
						CopyMem((APTR)myfib,(APTR) Fib,myfibsize);
						}
					}
				}
			else
				{
				UnLock(FLock);
				FreePooled(mem_pool,myfib,myfibsize);
				wb(NO_MEM);
        }
      FreePooled(mem_pool,myfib,myfibsize);
			}
		else
			{
			UnLock(FLock);
			wb(NO_MEM);
			};
//		Fib=quicksort(Fib);
    }
	else
		{
		wb("[37m\n\n\rI was not able to lock your UL Directory, plz inform SysOp!!![0m\n\n\r");
		}
  PutString("We have read the dir, now removing memory list\n\r",1);
	free_elements(Fib);
	PutString("Removal finished.\n\r",1);
	}

/****************************************************************************
 *  FUNCTION: free_elements()
 *   PURPOSE: Terminates the FAMEDoor session
 * PARAMETER: *l => Base Pointer of Filelist
 *   RETURNS: none
 ****************************************************************************/

void free_elements (struct Liste *l)
	{
	struct Liste *l_help1,*l_help2 ;

  l_help1 = l ;
  while(l_help1)
  	{
		l_help2 = l_help1 ;
    l_help1 = l_help1->next ;
    FreePooled(mem_pool,l_help2,sizeof(struct Liste));
  	}
	Fib=NULL;Fib2=NULL;
  }

