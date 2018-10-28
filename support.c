/**************************************************************************************************
 *  PROJECT: FAME-FTPd
 *     FILE: support.c
 *  PURPOSE: General support functions used frequently
 *  CREATED: 05-MAY-2003
 * MODIFIED: 25-JAN-2004
 *   AUTHOR: Sascha 'SieGeL' Pfalz
 **************************************************************************************************
 * The following functions are defined here:
 *
 * DebugLog()						- General debug logging
 * tcp_error()          - TCP error strings
 * FormatDateHookFunc()	- Hook to Formate Datestamps
 * FormatStamp()        - Creates Datestring
 * readline()           - Reads a line from a socket
 * recvchar()						- Recieves a char from a socket
 * usputc()							- Puts a char on a socket
 * usprintf()						- Formatted printf() on a socket
 * CutCRLF()						- Cuts CRLF from a string
 * ConvertSpaces()			- Converts Spaces to Underscores
 * GetFileSize()        - Returns size of a given filename
 * DNSLookUp() 					- Performs a DNS Lookup on a given IP Address
 * WeektopStats()				- Writes out stats files so that aCID-tOP can count the uploads
 **************************************************************************************************/

#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/locale.h>

#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <sys/types.h>
#include <sys/syslog.h>
#include <sys/socket.h>
#include <sys/errno.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/file.h>
#include <sys/wait.h>
#include <sys/errno.h>

#include <exec/exec.h>
#include <exec/nodes.h>
#include <exec/lists.h>

#include <utility/date.h>
#include <utility/tagitem.h>

#include <dos/dosextens.h>

#include <proto/socket.h>
#include <netinet/in.h>
#include <amitcp/socketbasetags.h>

#include <proto/syslog.h>
#include <libraries/syslog.h>

#include <proto/fame.h>
#include <libraries/fame.h>
#include <fame/fame.h>

#include <netdb.h>
#include "struct_ex.h"
#include "proto.h"

extern confh;

/**************************************************************************************************
 * Prototype declarations:
 **************************************************************************************************/

void 		DebugLog(char *fmt, ...);
STRPTR 	tcp_error(int error);
STRPTR 	InitStack(void);
STATIC 	LONG __saveds __asm FormatDateHookFunc(register __a0 struct Hook *Hook,register __a1 UBYTE Char);
BOOL 		__regargs FormatStamp(struct DateStamp *Stamp,STRPTR DateBuffer,struct Locale *loc,BOOL listdate);
int 		readline(int fd, char *bufptr, size_t len);
int 		usputc(LONG s,char c);
int 		usprintf(LONG s,short stripcrlf,char *fmt,...);
void 		CutCRLF(char *s);
void 		ConvertSpaces(char *s);
long		GetFileSize(char *fpath);
long 		GetFileSizeFH(BPTR fh);
long 		DNSLookUp(struct sockaddr_in *sin, STRPTR name);
void    WeektopStats(void);
void 		WriteWTS(struct AcidStats *stats, long confid);
void 		FAMELog(char *fmt, ...);

/**************************************************************************************************
 * FUNCTION: DebugLog()
 *  PURPOSE: Append an entry to our debugging Logfile
 *    INPUT: *fmt => sprintf-style format string
 *            ... => Variable arg list
 *   RETURN: -
 **************************************************************************************************/

void DebugLog(char *fmt, ...)
	{
	char		datebuf[LEN_DATSTRING*2];
	char		logname[256];
	BPTR		fhandle;

	if(*fconfig->LogFile)
		{
		FAMEStrCopy(fconfig->LogFile,logname,255);
    }
	else
		{
		FAMEStrCopy("RAM:ftpd_debug.log",logname,255);
    }
	if(fhandle=Open(logname,MODE_READWRITE))
		{
		bzero(datebuf,sizeof(datebuf));
		FormatStamp(NULL,datebuf,myloc,FALSE);
		Seek(fhandle,0,OFFSET_END);
		FPrintf(fhandle,"[%s]: ",datebuf);
		if(&fmt+1) VFPrintf(fhandle, fmt, (long *)(&fmt + 1));
    else FPuts(fhandle,fmt);
		if(confh != -1)
			{
			if(&fmt+1) VFPrintf(confh,fmt,(long *)(&fmt +1));
   	  else FPuts(confh,fmt);
			FPuts(confh,"\n");
			}
		if(SysLogBase)
			{
			if(&fmt+1)	Log(LOG_FTP|LOG_INFO, LOG_PID, "FAME-FTPd",fmt,(LONG *)(&fmt+1));
			else 	Log(LOG_FTP|LOG_INFO, LOG_PID, "FAME-FTPd",fmt,NULL);
			}
		if(fmt[strlen(fmt)-1]!='\n') FPuts(fhandle,"\n");
		Close(fhandle);
		}
	}

/**************************************************************************************************
 * FUNCTION: tcp_error()
 *  PURPOSE: Returns descriptive error text from TCP/IP stack
 *    INPUT: error => Last error occured on TCP
 *   RETURN: Descriptive error text
 *     NOTE: Taken from smbfs source. Thanks Olaf!
 **************************************************************************************************/

STRPTR tcp_error(int error)
	{
	struct TagItem tags[2];
	STRPTR result;

	tags[0].ti_Tag	= SBTM_GETVAL(SBTC_ERRNOSTRPTR);
	tags[0].ti_Data	= error;
	tags[1].ti_Tag	= TAG_END;
	SocketBaseTagList(tags);
	result = (STRPTR)tags[0].ti_Data;
	return(result);
	}

/**************************************************************************************************
 * FUNCTION: FormatDateHookFunc()
 *  PURPOSE: Hook for locale.library/FormatDate()
 *     NOTE: Used in FormatStamp()
 **************************************************************************************************/

STATIC LONG __saveds __asm FormatDateHookFunc(register __a0 struct Hook *Hook,register __a1 UBYTE Char)
	{
	STRPTR String=Hook->h_Data;
	*String++=Char;
	Hook->h_Data=String;
	return(TRUE);
	}

/**************************************************************************************************
 * FUNCTION: FormatStamp()
 *  PURPOSE: Creates a human-readable datestring
 *    INPUT: *Stamp			=> Datestamp to use or NULL for current date
 *           DateBuffer	=> Buffer to hold the created string
 *           *loc				=> Pointer to struct *Locale or NULL to not use Locale settings
 *           listdate		=> TRUE   = Include date in string
 *                      => FALLSE = Only include time informations
 *   RETURN: Always return TRUE
 *     NOTE: Requires FormatDateHookFunc() above!
 **************************************************************************************************/

BOOL __regargs FormatStamp(struct DateStamp *Stamp,STRPTR DateBuffer,struct Locale *loc,BOOL listdate)
	{
	struct DateStamp __aligned Now;
	struct Hook LocalHook={{NULL}, (HOOKFUNC)FormatDateHookFunc};
	struct DateTime __aligned	mydtime;
  char	 timebuf[LEN_DATSTRING];

	if(!Stamp) DateStamp(Stamp = &Now);
	if(loc)
		{
		if(DateBuffer)
			{
			LocalHook.h_Data=DateBuffer;
			if(listdate==FALSE)	FormatDate(loc,"%d-%b-%Y %H:%M:%S",Stamp,&LocalHook);
			else FormatDate(loc,"%b %d %H:%M",Stamp,&LocalHook);
			}
		}
	else
		{
		mydtime.dat_Stamp  	= *Stamp;
		mydtime.dat_Format	= FORMAT_USA;
    mydtime.dat_Flags		= NULL;
		mydtime.dat_StrDay	= NULL;
		mydtime.dat_StrDate	= DateBuffer;
		mydtime.dat_StrTime	= timebuf;
		if(!DateToStr(&mydtime)) return(FALSE);
		FAMEStrCat(" ",DateBuffer);
		FAMEStrCat(timebuf,DateBuffer);
		}
	return(TRUE);
	}

/**************************************************************************************************
 * FUNCTION: readline()
 *  PURPOSE: Reads a line from a socket (used for Command parsing from control socket)
 *    INPUT: fd			=> Socket from where to read
 *           bufptr	=> Pointer to memory buffer to hold the string
 *           len		=> How many bytes we can read
 *   RETURN: How many bytes where read, -1 in case of error
 **************************************************************************************************/

int readline(int fd, char *bufptr, size_t len)
	{
	char *bufx = bufptr;
	static char *bp;
	static int cnt = 0;
	char	c;

	FAMEFillMem(readtmp,0,CMDBUF_TEMP);
	while(--len > 0)
		{
		if(--cnt <=0)
			{
      cnt = recv(fd,readtmp,sizeof(readtmp), 0);
			if(cnt < 0)
				{
        if(Errno()==EINTR)
					{
					len++;		/* The while() will decrement */
					continue;
					}
				return(-1);
				}
			if(cnt == 0) return(0);
			bp = readtmp;
			}
		c = *bp++;
		*bufptr++ = c;
		if( c == '\n')
			{
     	*bufptr = '\0';
			return(bufptr - bufx);
			}
		}
	return(-1);
	}

/*************************************************************************************************
 * FUNCTION: usputc()
 *  PURPOSE: Buffered putchar to a socket
 *    INPUT: s => Socket where to write to
 *           c => Character to put on socket
 *   RETURN: -1 in case of Error else 0
 *************************************************************************************************/

int usputc(LONG s,char c)
	{
	return send(s,&c,1,0);
	}

/*************************************************************************************************
 * FUNCTION: usprintf()
 *  PURPOSE: printf() style writing on socket *
 *    INPUT:    s 			=> Socket where to send the data
 *           stripcrlf 	=> CRLF_STRIP will remove and add CRLF, CRLF_NOSTRIP just sends AS-IS
 *           *fmt 			=> sprintf() style Format String
 *            ... 			=> Variable arg list
 *   RETURN: Amount of bytes sent
 *************************************************************************************************/

int usprintf(long s,short stripcrlf,char *fmt, ...)
	{
	int len;

	FAMEFillMem(uspbuf,0,1024);
	RawDoFmt(fmt, (long *)(&fmt + 1), (void (*))"\x16\xc0\x4e\x75",uspbuf);
	if(stripcrlf==CRLF_STRIP)
		{
		CutCRLF(uspbuf);
		FAMEStrCat("\r\n",uspbuf);
		}
	len = send(s,uspbuf,strlen(uspbuf),0);
	return(len);
	}

/*************************************************************************************************
 * FUNCTION: CutCRLF()
 *  PURPOSE: Function to cut out CRLF chars
 *    INPUT: s => String where to cut the CRLF chars
 *   RETURN: -
 *     NOTE: Function modifies passed buffer directly!
 *************************************************************************************************/

void CutCRLF(char *s)
	{
	char *d=s;

	while(*d)
		{
		if(*d=='\n' || *d=='\r') *d++;
		else *s++=*d++;
		}
	*s='\0';
	}

/*************************************************************************************************
 * FUNCTION: ConvertSpaces()
 *  PURPOSE: Function to convert spaces to underscores
 *    INPUT: s => String where to convert the spaces
 *   RETURN: -
 *     NOTE: Function modifies passed buffer directly!
 *************************************************************************************************/

void ConvertSpaces(char *s)
	{
	char *d=s;

	while(*d)
		{
		if(*d==' ' || *d=='/' || *d==':')
			{
			*s++='_';
			*d++;
			}
		else *s++=*d++;
		}
	*s='\0';
	}

/**************************************************************************************************
 * FUNCTION: GetFileSize()
 *  PURPOSE: Returns size of a given filename by Open()ing the file and passing fh to GetFileSizeFH()
 *    INPUT: Name of file
 *   RETURN: Size of file or -1 in case of an error
 **************************************************************************************************/

long GetFileSize(char *fpath)
	{
	BPTR	testit;
	long	fsize;

	if(testit = Open(fpath,MODE_OLDFILE))
		{
		fsize = GetFileSizeFH(testit);
		Close(testit);
		return(fsize);
		}
	else
		{
		char errbuf[120];
	  Fault(IoErr(),NULL,errbuf,120);
  	DebugLog("GetFileSize(): %s: %s",fpath,errbuf);
		return(-1);
		}
	}

/**************************************************************************************************
 * FUNCTION: GetFileSizeFH()
 *  PURPOSE: Returns size of a given filehandle
 *    INPUT: Opened filehandle
 *   RETURN: Size of file or -1 in case of an error
 **************************************************************************************************/

long GetFileSizeFH(BPTR fh)
	{
	FAMEFillMem(fib,0,sizeof(struct FileInfoBlock));
	ExamineFH(fh,fib);
	return(fib->fib_Size);
	}

/**************************************************************************************************
 * FUNCTION: DNSLookUp()
 *  PURPOSE: Performs a DNS Lookup on a given IP Address.
 *    INPUT: ip   => IP Address to check (struct sockaddr_in *)
 *           name => Resolved hostname.
 *   RETURN: 0 if lookup was successful, else -1 to indicate error
 **************************************************************************************************/

long DNSLookUp(struct sockaddr_in *sin, STRPTR name)
	{
	struct 	hostent *host;
	int			retries = 0;

	while(retries < CONNECT_RETRY)
    {
		host = gethostbyaddr((char *)&sin->sin_addr,sizeof(struct in_addr),AF_INET);
		if(host)
			{
    	FAMEStrCopy(host->h_name,name,255);
			return(0);
    	}
		switch(h_errno)
			{
			case	TRY_AGAIN:	retries++;
												continue;
			default:					return(-1);   // Any other error is non-recoverable, so return

			}
		}
	}

/**************************************************************************************************
 * FUNCTION: WeektopStats()
 *  PURPOSE: Writes aCID-tOP compatible data so that uploads can be counted
 *    INPUT: -
 *   RETURN: -
 *    NOTES: This function is called AFTER (!) the socket is closed, no user communication
 *           is possible here except using EasyRequest()ers! See also main()
 **************************************************************************************************/

void WeektopStats(void)
	{
	struct	FTPUploads *f1;
	struct	AcidStats *astats;
  long		lv,retries;
	BOOL		needupdate = FALSE;
	static	char aslock[] = "T:FTPd_as.lock";
	BPTR		lockfp;

	if(!(astats = AllocPooled(mem_pool,sizeof(struct AcidStats) * (FameInfo->NodeCount+1))))
		{
		fail(ERROR_NO_FREE_STORE,"Cannot allocate AcidStats structure!!!");
		return;
		}
	for(f1=(struct FTPUploads *)filelist.mlh_Head;f1!=(struct FTPUploads *)&filelist.mlh_Tail;f1=(struct FTPUploads *)f1->Node.mln_Succ)
		{
		astats[f1->ConfNum].UserNumber = fstats->UserNumber;
		astats[f1->ConfNum].ULFiles++;
    FAMEAdd64(NULL,f1->FileSize,&astats[f1->ConfNum].BytesHi);
		needupdate = TRUE;
    }
	if(needupdate == FALSE) return;

	/* Stats exist, first create our Lockfile so we can safely update/create the stats */

	retries = lockfp = 0;
	while(1)
		{
		if(retries > WTS_RETRY)
			{
			fail(NULL,"Cannot lock to write aCID-tOP stats!");
			return;
			}
		lockfp = Lock(aslock,SHARED_LOCK);
		if(lockfp)
			{
			UnLock(lockfp);
			Delay(WTS_RETRY_DELAY);
			retries++;
			continue;
			}
		if(!(lockfp = Open(aslock,MODE_NEWFILE)))	/* File cannot be created, someone else was faster, wait */
			{
			Delay(WTS_RETRY_DELAY);
			retries++;
			continue;
			}
		break;
		}

	/* Lockfile is created and open, write out the stats */

	for(lv = 0; lv < FameInfo->NodeCount+1; lv++)
		{
    if(astats[lv].UserNumber)
			{
			WriteWTS(&astats[lv],lv);
			}
		}

	/* And now remove our lockfile so that other instances can update, too */

	Forbid();
	Close(lockfp);
	DeleteFile(aslock);
	Permit();
	}

/**************************************************************************************************
 * FUNCTION: WriteWTS()
 *  PURPOSE: Writes a member of AcidStats after aggregation of all uploaded files
 *    INPUT: stats	=> Pointer to one member of struct AcidStats
 *           confid => The conference id for which these stats are counted
 *   RETURN: -
 **************************************************************************************************/

void WriteWTS(struct AcidStats *stats, long confid)
	{
	static	char asname[] = "FAME:ExternEnv/Doors/FTPd-AT_%ld.dat";
	BPTR		statsfp;
	char		fname[128];
	long		size,lv;
	struct	AcidStats	*astemp;
	BOOL		foundit = FALSE;

	if(!(astemp = AllocPooled(mem_pool,sizeof(struct AcidStats))))
		{
		DebugLog("WriteWTS(): Cannot allocate temporary filebuffer!");
		return;
		}
  SPrintf(fname,asname,confid);
  statsfp = Open(fname,MODE_READWRITE);
	if(!statsfp)
		{
		DebugLog("WriteWTS(): Cannot open %s for writing!",fname);
    FreePooled(mem_pool,astemp,sizeof(struct AcidStats));
		return;
		}
	size = GetFileSizeFH(statsfp);
	if(size)				/* Members inside our datafile, so search for the right file and seek to position: */
		{
		for(lv = 0; lv < (size/sizeof(struct AcidStats)); lv++)
			{
			Seek(statsfp,lv * sizeof(struct AcidStats),OFFSET_BEGINNING);
			if(!Read(statsfp,astemp,sizeof(struct AcidStats)))
				{
        Close(statsfp);
		    FreePooled(mem_pool,astemp,sizeof(struct AcidStats));
        DebugLog("WriteWTS(): Error reading existing datafile %s!",fname);
				return;
				}
			if(astemp->UserNumber == stats->UserNumber)
				{
      	Seek(statsfp, lv * sizeof(struct AcidStats), OFFSET_BEGINNING);
				foundit = TRUE;
				stats->ULFiles+=astemp->ULFiles;
				FAMEAdd64(astemp->BytesHi,astemp->BytesLo,&stats->BytesHi);
				break;
				}
			}
    if(foundit == FALSE) 
			{
			Seek(statsfp, 0L, OFFSET_END);
			}
		}
	else
		{
    Seek(statsfp,0L,OFFSET_BEGINNING);
		}
	if(Write(statsfp,stats,sizeof(struct AcidStats))!=sizeof(struct AcidStats))
		{
    Close(statsfp);
		DeleteFile(fname);
    FreePooled(mem_pool,astemp,sizeof(struct AcidStats));
		DebugLog("WriteWTS(): Writing to %s failed - File removed!",fname);
		return;
		}
	Close(statsfp);
  FreePooled(mem_pool,astemp,sizeof(struct AcidStats));
	}

