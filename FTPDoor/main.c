/****************************************************************************************
 *  PROJECT: FAME-FTPd / FTPDoor
 *     FILE: main.c
 *  PURPOSE: Main file of FTPDoor - Multi-Purpose FAME door for FTPd
 *  CREATED: 31-AUG-2003
 * MODIFIED: 24-APR-2004
 *   AUTHOR: Sascha 'SieGeL' Pfalz
 ****************************************************************************************/

#include <exec/exec.h>
#include <Fame/fameDoorProto.h>
#include <pragmas/utility_pragmas.h>
#include <pragmas/date_pragmas.h>
#include <proto/locale.h>
#include <proto/date.h>
#include <libraries/date.h>
#include <utility/date.h>
#include <utility/tagitem.h>
#include "version.h"
#include "/struct.h"
#include "/global_defines.h"

#define	CMD_FTPWHO		0				// Who is online command (FTPWHO)
#define CMD_FTPINIT		1				// Initial check for new files command (FTPINIT)

/****************************************************************************
 * Global variables
 ****************************************************************************/

const 	char *ver="$VER: FTPDoor "COMPILE_VERSION" ("COMPILE_DATE") ["CPU_TYPE"]\0";
static 	char *doorcommands[]={"FTPWHO","FTPINIT",NULL};
long		node;

/****************************************************************************
 * PROTOTYPES
 ****************************************************************************/

void main(void);
void __autoopenfail(void) { _XCEXIT(0);}	// Dummy function for SAS
void CloseLibs(void);
void ShutDown(long error);
void wb(char *err);

static void FTPWho(void);
static void ShowWhoList(struct FTPdStats *, long entries);

extern void FTPInit(void);								// Defined in init.c

/****************************************************************************
 *  FUNCTION: main()
 *   PURPOSE: Main entry of tool
 * PARAMETER: none
 *   RETURNS: none
 ****************************************************************************/

void main(void)
	{
	struct 	RDArgs *rda=NULL;
	long    ArgArray[1]={0L};
	char		iobuf[202],**lv;
	int			cmd = 0,actcommand = -1;

	if(rda=ReadArgs("NODE-NR./A/N",ArgArray,rda))
		{
		node=*(LONG *)ArgArray[0];
		SPrintf((STRPTR)FAMEDoorPort,"FAMEDoorPort%ld",node);
		FreeArgs(rda);
		}
	else
		{
		FreeArgs(rda);
		PrintFault(IoErr(),(FilePart(_ProgramName)));
		Printf("\n%s is a FAME BBS-Door and is only usable via the BBS !\n\n",(FilePart(_ProgramName)));
		exit(0);
		}
	if(!(mem_pool=CreatePool(MEMF_CLEAR|MEMF_PUBLIC,20480L,20480L))) { SetIoErr(ERROR_NO_FREE_STORE);CloseLibs();}
	if(!(UtilityBase=(struct Library *) 	OpenLibrary("utility.library",37L))) { SetIoErr(ERROR_INVALID_RESIDENT_LIBRARY);exit(20);}
	if(!(FAMEBase=(struct FAMELibrary *) 	OpenLibrary(FAMENAME,5L))) { SetIoErr(ERROR_INVALID_RESIDENT_LIBRARY);CloseLibs();}
	if(!(LocaleBase=(struct LocaleBase *)	OpenLibrary("locale.library",38L))) { SetIoErr(ERROR_INVALID_RESIDENT_LIBRARY);CloseLibs();}
	myloc=OpenLocale(NULL);
	if(FIMStart(0,0UL)) CloseLibs();
	if(!(fib = AllocDosObject(DOS_FIB,NULL)))
		{
		wb("[37mCANNOT ALLOCATE FIB STRUCTURE!!!");
		}
	GetCommand(iobuf,0,0,0,NR_GetDoorCallName);
  lv = doorcommands;
	while(*lv!=NULL)
		{
    if(!Stricmp(iobuf,*lv))
			{
			actcommand = cmd;
			break;
			}
		cmd++;
		*lv++;
		}
	if(actcommand == -1)
		{
		PutStringFormat("\n\r[37mFTPDoor: Unknown Callmode %s specified - Plz read docs!!\r\n[m",iobuf);
		wb("\r\n");
		}
	switch(actcommand)
		{
		case  CMD_FTPWHO:		FTPWho();
												break;
		case	CMD_FTPINIT:	FTPInit();
												break;
		}
	wb("");
	}
/****************************************************************************
 *  FUNCTION: wb()
 *   PURPOSE: Terminates the FAMEDoor session
 * PARAMETER: none
 *   RETURNS: none
 ****************************************************************************/

void wb(char *err)
	{
	if(*err) NC_PutString(err,1);
	PutStringFormat("[m\n\r");
	FIMEnd(0);
	}

/****************************************************************************
 *   FUNCTION: ShutDown()
 *    PURPOSE: Called by FAME Door header to shutdown door, calls CloseLibs()
 * PARAMETERS: error => Error code from FAME, see FAMEDefines.h
 *    RETURNS: none
 ****************************************************************************/

void ShutDown(long error)
	{
	CloseLibs();
	}

/****************************************************************************
 *  FUNCTION: CloseLibs()
 *   PURPOSE: Closes all libs and frees all resources
 * PARAMETER: none
 *   RETURNS: none
 ****************************************************************************/

void CloseLibs(void)
	{
	if(fib)						FreeDosObject(DOS_FIB,fib); fib = NULL;
	if(UtilityBase)		CloseLibrary(UtilityBase); UtilityBase=NULL;
  if(LocaleBase)
		{
		if(myloc) CloseLocale(myloc);myloc=NULL;
		CloseLibrary((struct Library *)LocaleBase);LocaleBase=NULL;
		};
	if(FAMEBase) 			CloseLibrary((struct Library *)FAMEBase); FAMEBase=NULL;
	if(mem_pool)      DeletePool(mem_pool); mem_pool = NULL;
	exit(0);
	}

/****************************************************************************
 *  FUNCTION: FTPWho()
 *   PURPOSE: Handles FTPWho() Command
 * PARAMETER: none
 *   RETURNS: none
 ****************************************************************************/

static void FTPWho(void)
	{
	struct  FTPdStats *mydata;
	long    myentries,cnt = 0, i;
	BPTR		fp;
	static	char noconns[]="\n\r[32mCurrently there are no active FTP connections.[m\n\r";
	char		headbuf[200];

	PutString("\f\n\r",0);
	SPrintf(headbuf,"FTPWho [31mv%s [36mby Sascha 'SieGeL/tRSi' Pfalz[m",COMPILE_VERSION);
	Center(headbuf,1,35);
	PutString("\n\r",1);
	PutString("[33mUsername           [34m|  [33mConnection Time  [34m| [33mAction[m",1);
	PutString("[34m-------------------+-------------------+-------------------------------------[m",1);
	while(cnt < STATS_FILE_RETRY)
		{
		if(!(fp = Open(statname,MODE_OLDFILE)))		// File is accessed by many instances!
			{
			if(IoErr()==ERROR_OBJECT_NOT_FOUND)			// IoErr() = 205, this is okay and not treated as error
				{
				PutString(noconns,1);
				return;
        }
			cnt++;
			Delay(STATS_RETRY_DELAY);
			continue;
			}
		ExamineFH(fp,fib);
		myentries = fib->fib_Size / sizeof(struct FTPdStats);
		if(!myentries)														// File has 0 entries, okay as it shows no active connections
			{
    	PutString(noconns,1);
			Close(fp);
			return;
			}
		if(!(mydata = AllocPooled(mem_pool,sizeof(struct FTPdStats) * (myentries+1))))
			{
    	PutString("\n\r[37mCannot allocate memory for statistics??\n\r",1);
			Close(fp);
			return;
			}
    for(i = 0; i < myentries; i++)
			{
      if(!Read(fp,&mydata[i],sizeof(struct FTPdStats)))
				{
        PutStringFormat("\r\n[37mError reading statsfile: %ld\n\r",IoErr());
				Close(fp);
				return;
				}
			}
		Close(fp);
		ShowWhoList(mydata,myentries);
		return;
		}
	PutStringFormat("[37mError while accessing stats file: %ld\n\r[m",IoErr());		// This should never happen!
	}

/****************************************************************************
 *  FUNCTION: ShowWhoList()
 *   PURPOSE: Displays list of active users
 * PARAMETER: none
 *   RETURNS: none
 ****************************************************************************/

static void ShowWhoList(struct FTPdStats *dat, long entries)
	{
  long	i;
	struct	FAMEUser *fuser;
	struct	FAMEUserKeys *fkeys;

	if(!(fuser = AllocPooled(mem_pool, sizeof(struct FAMEUser))))
		{
    wb("[37mNo memory left for user.data structure?[m");
		}
	if(!(fkeys = AllocPooled(mem_pool, sizeof(struct FAMEUserKeys))))
		{
    wb("[37mNo memory left for user.keys structure?[m");
		}
	for(i = 0; i < entries; i++)
		{
		if(dat[i].UserNumber)
			{
			MyFAMEDoorMsg->fdom_StructDummy1 = fuser;
			MyFAMEDoorMsg->fdom_StructDummy2 = fkeys;
			GetCommand("",dat[i].UserNumber,0,0,RD_LoadAccount);
			}
		else
			{
      FAMEStrCopy("N/A!",fuser->UserName,31);
			}
		PutStringFormat("[33m%-18s [34m| [33m%17s [34m| [32m%-36s\n\r",fuser->UserName,&dat[i].ConnectTime,&dat[i].ActionString);
		}
	}

