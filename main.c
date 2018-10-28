/****************************************************************************************
 *  PROJECT: FAME-FTPd
 *     FILE: main.c
 *  PURPOSE: Main file of FAME FTPd - A FTP server for FAME BBS systems
 *  CREATED: 05-MAY-2003
 * MODIFIED: 24-APR-2004
 *   AUTHOR: Sascha 'SieGeL' Pfalz
 *    NOTES: FTP parts taken from JornFTPd written by
 *           Joran Jessurun <nl0jor@nl0jor.nl.cbpr.org> THX!
 *           Also some codeparts are taken and adapted from wu-ftpd
 ****************************************************************************************/

#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/locale.h>
#include <proto/utility.h>
#include <proto/intuition.h>
#include <stdlib.h>
#include <string.h>
#include <dos/stdio.h>
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
#include <error.h>
#include <exec/exec.h>
#include <exec/nodes.h>
#include <exec/lists.h>
#include <utility/date.h>
#include <utility/tagitem.h>
#include <proto/socket.h>
#include <netinet/in.h>
#include <amitcp/socketbasetags.h>
#include <netdb.h>
#include <arpa/nameser.h>
#include <netinet/in.h>
#include <proto/fame.h>
#include <libraries/fame.h>
#include <fame/fame.h>
#include <proto/syslog.h>
#include <libraries/syslog.h>
#include <lineread.h>
#include <devices/timer.h>

#include "version.h"
#include "proto.h"
#include "ftp.h"
#include "struct.h"

const 	char *ver="$VER: FAME-FTPd "COMPILE_VERSION" ("COMPILE_DATE") ["CPU_TYPE"]\0";
const 	char *dirfmt = "%lcrw-rw-rw- %3ld %s users %ld %s %s\r\n";    // Used to display directory
const 	int ONE = 1;
static	char lockfile[]="T:FTPd.lock";

int			errno;								// Global error var for tcp errors
int			h_errno;           		// Errno for host errors
UBYTE 	program_name[256];		// Our own programname
BPTR 		confh = -1;						// Debug window ptr
char		PassiveIP[24];				// For NAT Systems this is the "real" IP to use for passive connects
char		ipbuf[256];         	// Will contain the contents from the IPFile
USHORT	ActivePortCounter;		// This will be initialized to the first allowed port or set to 0 for auto-detection
int			rcvbufsz = 16*1024;  	// The default SO_RCVBUF size used in setsockopt() calls

/*****************************************************************************************
 * PROTOTYPES
 *****************************************************************************************/

int 		main(int argc, char argv[]);
int			ftpmain(void);
void		CloseLibs(void);
void 		fail(long iocode, char *errstring, ...);
long 		sendfile(BPTR fp,LONG s,int mode, BOOL sendCRLF);
long 		recvfile(BPTR fp,LONG s,int mode);
long 		DataSocket(struct FTP *ftp,char *path, short mode);

static 	void 	FTPLogin(struct FTP *ftp, char *pass);
static 	int 	pport(struct FTP *ftp,char *arg);
static 	void 	Passive(struct FTP *ftp);
static 	void 	FTPList(struct FTP *ftp, char *arg);
static 	void 	ChangeDirectory(struct FTP *ftp, char *arg);
static 	long 	ShowDirContents(char *arg, BPTR fh,struct FTP *ftp);
static 	void 	SendTransferStats(long s);
static  void	HandleDownload(struct FTP *ftp,char *arg);
static  void	HandleSize(struct FTP *ftp,char *arg);
static  void  HandleUpload(struct FTP *ftp, char *arg);
static	void	HandleFeatures(struct FTP *ftp);
static 	void 	HandleClient(struct FTP *ftp, char *arg);
static 	void 	HandleHelp(struct FTP *ftp, char *arg);
static 	void 	InitStats(struct FTP *ftp);
static 	BOOL 	HandleStats(char *actionstring,...);
static 	BOOL 	RemoveFromStats(void);
static  void	HandleStat(struct FTP *ftp, char *arg);
static 	long 	CountOnlineUsers(void);

/****************************************************************************************
 * FUNCTION: main()
 *  PURPOSE: Main entry point
 *    INPUT: argc => Argument count
 *           argv => Argument array
 *   RETURN: Returncode for shell/inetd (0)
 ****************************************************************************************/

int main(int argc, char argv[])
	{
	int 		error;
	BPTR		prefsptr;

	if(!(IntuitionBase=(struct IntuitionBase *) OpenLibrary("intuition.library",39L)))
		{
		DebugLog("Cannot open intuition.library V39+!!!");
		SetError(ERROR_INVALID_RESIDENT_LIBRARY);
		CloseLibs();
		}
	if(!(SocketBase=(struct Library *) OpenLibrary(SOCKETNAME,4L)))
		{
		fail(ERROR_INVALID_RESIDENT_LIBRARY,"Cannot open %s V4+ - Please start TCP/IP first!!!",SOCKETNAME);
		SetError(ERROR_INVALID_RESIDENT_LIBRARY);
		CloseLibs();
		}
	SysLogBase = OpenLibrary(SYSLOGNAME, SYSLOGVERSION);	// No check required, if it does not exist we simply ignore Log()
	GetProgramName(program_name,sizeof(program_name));
	error = SocketBaseTags(	SBTM_SETVAL(SBTC_ERRNOPTR(sizeof(errno))),	&errno,
													SBTM_SETVAL(SBTC_HERRNOLONGPTR),						&h_errno,
													SBTM_SETVAL(SBTC_LOGTAGPTR),								program_name,
													SBTM_SETVAL(SBTC_BREAKMASK),								SIGBREAKF_CTRL_C,
													TAG_END);
	if(error != 0)
		{
		fail(0,"Could not initialize 'bsdsocket.library' (%ld, %s)",error,tcp_error(error));
		CloseLibs();
		}
	if(!(UtilityBase=(struct Library *) OpenLibrary("utility.library",37L)))
		{
		fail(ERROR_INVALID_RESIDENT_LIBRARY,"Cannot open utility.library V37+!!!");
		SetError(ERROR_INVALID_RESIDENT_LIBRARY);
		CloseLibs();
		}
	if(!(LocaleBase=(struct LocaleBase *)OpenLibrary("locale.library",38L)))
		{
		fail(ERROR_INVALID_RESIDENT_LIBRARY,"Cannot open locale.library V38+!!!");
		SetError(ERROR_INVALID_RESIDENT_LIBRARY);
		CloseLibs();
		}
	if(!(FAMEBase=(struct FAMELibrary *)OpenLibrary(FAMENAME,5L)))
		{
		fail(ERROR_INVALID_RESIDENT_LIBRARY,"Cannot open FAME.library V5++!!!");
		SetError(ERROR_INVALID_RESIDENT_LIBRARY);
    CloseLibs();
		}
	if(!(fib = AllocDosObject(DOS_FIB,NULL)))
		{
		fail(IoErr(),"AllocFIB failed!!!");
		SetError(ERROR_INVALID_RESIDENT_LIBRARY);
    CloseLibs();
		}
	if(!(mem_pool=CreatePool(MEMF_CLEAR|MEMF_PUBLIC,20480L,20480L)))
		{
		fail(ERROR_NO_FREE_STORE,"Mem pool alloc failed!");
		SetError(ERROR_NO_FREE_STORE);
		CloseLibs();
		}
	if(!(readbuf = AllocPooled(mem_pool,CMDBUF_SIZE)))
		{
		fail(ERROR_NO_FREE_STORE,"readbuf alloc failed!!!");
		SetError(ERROR_NO_FREE_STORE);
		CloseLibs();
		}
	if(!(readtmp = AllocPooled(mem_pool,CMDBUF_TEMP)))
		{
		fail(ERROR_NO_FREE_STORE,"readtmp alloc failed!!!");
		SetError(ERROR_NO_FREE_STORE);
		CloseLibs();
		}
	if(!(filebuf = AllocPooled(mem_pool,BLKSIZE)))
		{
		fail(ERROR_NO_FREE_STORE,"filebuf alloc failed!!!");
		SetError(ERROR_NO_FREE_STORE);
		CloseLibs();
		}
	if(!(uspbuf = AllocPooled(mem_pool,CMDBUF_SIZE)))
		{
		fail(ERROR_NO_FREE_STORE,"uspbuf alloc failed!!!");
		SetError(ERROR_NO_FREE_STORE);
		CloseLibs();
		}
	if(!(FameInfo = AllocPooled(mem_pool,sizeof(struct FAMEInfos))))
		{
		fail(ERROR_NO_FREE_STORE,"FameInfo alloc failed!!!");
		SetError(ERROR_NO_FREE_STORE);
		CloseLibs();
		}
	if(!(fstats = AllocPooled(mem_pool,sizeof(struct FTPdStats))))
		{
		fail(ERROR_NO_FREE_STORE,"FTPdStats alloc failed!!!");
		SetError(ERROR_NO_FREE_STORE);
		CloseLibs();
		}
	if(!(tmpptr = AllocPooled(mem_pool,sizeof(struct FTPdStats))))
		{
		fail(ERROR_NO_FREE_STORE,"FTPdStats2 alloc failed!!!");
		SetError(ERROR_NO_FREE_STORE);
		CloseLibs();
		}
	if(!(fconfig = AllocPooled(mem_pool,sizeof(struct FTPdConfig))))
		{
		fail(ERROR_NO_FREE_STORE,"fconfig alloc failed!!!");
		SetError(ERROR_NO_FREE_STORE);
		CloseLibs();
		}
	if(prefsptr=Open(prefspath,MODE_OLDFILE))
		{
		Read(prefsptr,fconfig,sizeof(struct FTPdConfig));
		Close(prefsptr);
		}
	else
		{
    FAMEStrCopy("RAM:",fconfig->UploadTempPath,255);		// Default Upload path is RAM:
		fconfig->FileNameLength = 12;                       // Default length is 12
		}
	if(!fconfig->FileBufferSize) fconfig->FileBufferSize = 512;	// Set to 512k copy buffer if not defined
	if(fconfig->Flags1 & CFG_USEDEBUG)
		{
		if(!(confh = Open("CON:0/0/680/250/FTPOUT/CLOSE/WAIT",MODE_NEWFILE)))
			{
	    CloseLibs();
			}
		}
	if(!fconfig->Timeout) fconfig->Timeout = 600;					// Default timeout is 10minutes (600s)
	if(*fconfig->IPFile)
		{
		if(prefsptr = Open(fconfig->IPFile,MODE_OLDFILE))
			{
    	FGets(prefsptr,ipbuf,CMDBUF_SIZE);
			Close(prefsptr);
			if(strlen(ipbuf) > 2)
				{
	      if(ipbuf[strlen(ipbuf)-1] == '\n')
					{
					ipbuf[strlen(ipbuf)-1] = '\0';
					}
				}
			else
				{
        *ipbuf = '\0';
				}
      }
		else
			{
			*ipbuf = '\0';
      }
		}
	else
		{
    *ipbuf = '\0';
		}
	if(fconfig->PortLo != 0 && fconfig->PortHi != 0)
		{
    ActivePortCounter = fconfig->PortLo;					// Initialize Port counter to start val
		}
	else
		{
		ActivePortCounter = 0;												// Let the system decide
		}
  NewList((struct List *)&filelist);		// Initialize the FileList structure
	myloc=OpenLocale(NULL);
	DebugLog("***** FAME-FTPd v%s started. (%08lx) *****\n",COMPILE_VERSION,FindTask(NULL));
	ftpmain();
	if(fconfig->Flags1 & CFG_WEEKTOP)
		{
		WeektopStats();
		}
	DebugLog("***** FAME-FTPd v%s stopped. (%08lx) *****\n",COMPILE_VERSION,FindTask(NULL));
	CloseLibs();
	}

/************************************************************************************************
 * FUNCTION: ftpmain()
 *  PURPOSE: Main loop, all TCP/IP communication is handled here
 *    INPUT: -
 *   RETURN: -
 ************************************************************************************************/

int ftpmain(void)
	{
	fd_set	readfds;
	struct	sockaddr_in peer;
	long		cnt,peerlen = sizeof(peer);
	long		addrlen;
	int			rc;
	struct	FTP *myftp;
	char 		**cmdp,*arg;
  BOOL		end = FALSE;
	struct	timeval ftptimeout;
	BPTR		titleptr;
	struct  hostent *iphost;
	LONG  	ipinfo;

	server_socket = init_inet_daemon();
  if(server_socket == -1)
		{
		fail(0,"Invalid socket - Maybe not started from inetD?!");
		return(-1);
		}
	if(!(myftp = AllocPooled(mem_pool,sizeof(struct FTP))))
		{
		fail(ERROR_NO_FREE_STORE,"myftp alloc failed!!!");
		return(-2);
		}
	myftp->data 		= -1;
	myftp->pdata 		= -1;
  myftp->control	= server_socket;
	FAMEFillMem(&peer,0,sizeof(struct sockaddr_in));
	getpeername(server_socket,(struct sockaddr *)&myftp->port,&peerlen);
	myftp->port.sin_port = IPPORT_FTPD;
  addrlen = sizeof(myftp->ctrl);
  if (getsockname(server_socket, (struct sockaddr *) &myftp->ctrl, &addrlen) < 0)
		{
		fail(0,"getsockname(): %s (%ld)",tcp_error(Errno()),Errno());
		usprintf(server_socket,CRLF_NOSTRIP,ftperror);
		return(-2);
    }
  FAMEStrCopy(Inet_NtoA(myftp->port.sin_addr.s_addr),myftp->IPaddr,21);
	if(RetrieveFAMEInfos())
		{
		fail(0,"Cannot connect to FAME, please start BBS System first !!!");
		usprintf(server_socket,CRLF_NOSTRIP,ftperror);
		return(-3);
		}
	if(fconfig->Flags1 & CFG_USEDNS)
		{
		if(!DNSLookUp(&myftp->port,myftp->hostname)) DebugLog("Connection from %s (%s) accepted.",myftp->hostname,myftp->IPaddr);
		else DebugLog("Connection from %s accepted",myftp->IPaddr);
    }
	else
		{
		DebugLog("Connection from %s accepted",myftp->IPaddr);
		}
	if(*ipbuf!='\0')	// We have a valid IP/Hostname, try to resolve it:
		{
		iphost = gethostbyname(ipbuf);
		if(iphost)
			{
      memcpy( &ipinfo, &iphost->h_addr_list[ 0 ][ 0 ], 4 );
      FAMEStrCopy(Inet_NtoA(ipinfo),PassiveIP,24);
			}
    else
			{
			fail(0,"Unable to resolve %s !!!",ipbuf);
			SetError(ERROR_OBJECT_NOT_FOUND);
			return(-4);
			}
		}
	if(fconfig->MaxUsers)			// We have to check how many users are currently connected:
		{
		cnt = CountOnlineUsers();
		if(cnt >= fconfig->MaxUsers)
			{
			usprintf(server_socket,CRLF_NOSTRIP,"421 Maximum usercount exceeded! Please try again l8er.\r\n");
			shutdown(myftp->control,2);
			CloseSocket(myftp->control);
			server_socket = -1;
			return(0);
			}
		}
	usprintf(server_socket,CRLF_NOSTRIP,banner,FameInfo->BBSName,FameInfo->FAMEVersion,FameInfo->FAMERevision,COMPILE_VERSION,COMPILE_BUILD);
	if(*fconfig->TitleFile)
		{
		if(titleptr = Open(fconfig->TitleFile,MODE_OLDFILE))
			{
			while(FGets(titleptr,readbuf,CMDBUF_SIZE))
				{
        usprintf(myftp->control,CRLF_STRIP,"220- %s",readbuf);
				}
			Close(titleptr);
			}
		}
	usprintf(server_socket,CRLF_NOSTRIP,"220 \r\n");
	InitStats(myftp);
	HandleStats("Connection from %s",myftp->IPaddr);
	FD_ZERO(&readfds);
	ftptimeout.tv_secs = fconfig->Timeout;
	ftptimeout.tv_micro = 0;
	// Main FTP loop starts here

	for(;;)
		{
	 	FD_SET(server_socket,&readfds);
   	rc = WaitSelect(server_socket+1,&readfds,NULL,NULL,&ftptimeout,NULL);
		if(rc < 0) break;
		if(!rc)
			{
			usprintf(myftp->control,CRLF_NOSTRIP,bye);
			DebugLog("Timeout recieved for %s, exiting.",myftp->IPaddr);
			break;
			}
		FAMEFillMem(readbuf,0UL,CMDBUF_SIZE);
		if(!FD_ISSET(server_socket,&readfds))
			{
			break;
			}
		cnt = readline(server_socket,readbuf,CMDBUF_SIZE);
		if(cnt<=0)
			{
			break;
    	}
		CutCRLF(readbuf);

		if(Strnicmp(readbuf,"PASS",4)) 
			{
			if(fconfig->Flags1 & CFG_USEDEBUG)
				{
				DebugLog("readbuf=|%s|",readbuf);
  			}
			}

   	/* Find command in table; if not present, return syntax error */

   	for(cmdp=commands;*cmdp != NULL;cmdp++)
			{
			if(!Strnicmp(*cmdp,readbuf,strlen(*cmdp))) break;
			}
   	if(*cmdp == NULL)
			{
   		usprintf(myftp->control,CRLF_NOSTRIP,badcmd);
   		continue;
   		}
		if(*myftp->username=='\0' || *myftp->password=='\0')
			{
	 		switch(cmdp-commands)
				{
 				case CMD_USER:
	 			case CMD_PASS:
 				case CMD_QUIT:	break;
 				default:				usprintf(myftp->control,CRLF_NOSTRIP,notlog);
	 											continue;
 				}
			}
   	arg = &readbuf[strlen(*cmdp)];
   	while(*arg == ' ') arg++;
		switch(cmdp-commands)
			{
   		case CMD_QUIT:  SendTransferStats(myftp->control);
											usprintf(myftp->control,CRLF_NOSTRIP,bye);
                      end = TRUE;
											break;

			case CMD_USER:	FAMEStrCopy(arg,myftp->username,31);
											if(*myftp->username=='\0')
												{
                        usprintf(myftp->control,CRLF_NOSTRIP,"332- Need account for login.\r\n");
												}
											else
												{
												usprintf(myftp->control,CRLF_NOSTRIP,givepass);
												}
											break;

			case CMD_PASS:	if(*myftp->username=='\0')
												{
												usprintf(myftp->control,CRLF_NOSTRIP,userfirst);
          							}
											else
        								{
                        FTPLogin(myftp,arg);
												}
											break;

			case CMD_SYST:	usprintf(myftp->control,CRLF_NOSTRIP,systreply);
											break;

   		case CMD_PORT:	if(pport(myftp,arg) == -1) usprintf(myftp->control,CRLF_NOSTRIP,badport);
   										else usprintf(myftp->control,CRLF_NOSTRIP,portok);
   										break;

			case CMD_NOOP:	usprintf(myftp->control,CRLF_NOSTRIP,okay);
                      break;

   		case CMD_TYPE:	switch(arg[0])
   											{
   											case 'A':
   											case 'a': myftp->type = ASCII_TYPE;									/* ASCII */
   																usprintf(myftp->control,CRLF_NOSTRIP,typeok,arg);
   																break;

   											case 'l':
   											case 'L': while(*arg != ' ' && *arg != '\0') arg++;	/* LOGICAL */
   																if(*arg == '\0' || *++arg != '8')
   																	{
   																	usprintf(myftp->control,CRLF_NOSTRIP,only8);
   																	break;
   																	}
   																myftp->type = LOGICAL_TYPE;
   																myftp->logbsize = 8;
   																usprintf(myftp->control,CRLF_NOSTRIP,typeok,arg);
   																break;

   											case 'B':
   											case 'b':
   											case 'I':
   											case 'i': myftp->type = IMAGE_TYPE;															/* BINARY/IMAGE */
   																usprintf(myftp->control,CRLF_NOSTRIP,typeok,arg);
   																break;

   											default:  usprintf(myftp->control,CRLF_NOSTRIP,badtype,arg);		/* INVALID TYPE */
   																break;
   											}
   										break;

			case CMD_PASV:	Passive(myftp);
											break;

			case CMD_PWD:
			case CMD_XPWD:  usprintf(myftp->control,CRLF_NOSTRIP,pwdmsg,myftp->cd);
   										break;

			case CMD_CWD:
			case CMD_XCWD:	ChangeDirectory(myftp,arg);
											break;

      case CMD_CDUP:	ChangeDirectory(myftp,"..");
											break;

			case CMD_LIST:	FTPList(myftp,arg);
                      break;

			case CMD_RETR:	HandleDownload(myftp,arg);
                      break;

			case CMD_SIZE:  HandleSize(myftp,arg);
											break;

      case CMD_STOR:	HandleUpload(myftp,arg);
											break;

			case CMD_STRU:	if(!Strnicmp("F",arg,1)) usprintf(myftp->control,CRLF_NOSTRIP,stru_ok);
											else usprintf(myftp->control,CRLF_NOSTRIP,stru_fail);
											break;

			case CMD_MODE:  if(!Strnicmp("S",arg,1)) usprintf(myftp->control,CRLF_NOSTRIP,mode_ok);
											else usprintf(myftp->control,CRLF_NOSTRIP,mode_fail);
											break;

			case CMD_ABOR:	if(myftp->pdata!=-1)
												{
												shutdown(myftp->pdata,2);
												CloseSocket(myftp->pdata);
												myftp->pdata = -1;
												}
											if(myftp->data!=-1)
												{
												shutdown(myftp->data,2);
												CloseSocket(myftp->data);
												myftp->data = -1;
												}
                      usprintf(myftp->control,CRLF_NOSTRIP,abor_ok);
											break;

			case CMD_FEAT:	HandleFeatures(myftp);
											break;

			case CMD_CLNT:	HandleClient(myftp,arg);
											break;

			case CMD_HELP:	HandleHelp(myftp,arg);
											break;

			case CMD_STAT:	HandleStat(myftp,arg);
                      break;

			default:				HandleStats("User is idle");
											usprintf(myftp->control,CRLF_NOSTRIP,badcmd);
                      break;

			}
		if(end==TRUE) break;
		}
	RemoveFromStats();
	if(myftp->pdata!=-1)
		{
    shutdown(myftp->pdata,2);
		CloseSocket(myftp->pdata);
		myftp->pdata=-1;
		}
	if(myftp->data!=-1)
		{
    shutdown(myftp->data,2);
		CloseSocket(myftp->data);
		myftp->data=-1;
		}
	if(end==FALSE)
		{
		SendTransferStats(-1);		// We where kicked out, log transfer stats
		}
	shutdown(myftp->control,2);
	CloseSocket(myftp->control);
	server_socket = -1;
	return(0);
	}

/************************************************************************************************
 * FUNCTION: FTPLogin()
 *  PURPOSE: Handles FTP Login (username must be in ftp->username, pass in *pass)
 *    INPUT: ftp	=> Master FTP structure
 *           pass	=> Supplied userpassword
 ************************************************************************************************/

static void FTPLogin(struct FTP *ftp, char *pass)
	{
	if(ValidateFAMEUser(ftp->username,pass,ftp)==FALSE)
		{
		usprintf(ftp->control,CRLF_NOSTRIP,noperm);
		*ftp->username='\0';
		*ftp->password='\0';
		return;
		}
  else
		{
		if(CheckIfOnline(ftp->usernumber)==TRUE)
			{
      usprintf(ftp->control,CRLF_NOSTRIP,onlinewarn);
      *ftp->username='\0';
			*ftp->password='\0';
      return;
			}
		FAMEStrCopy(pass,ftp->password,31);
		usprintf(ftp->control,CRLF_NOSTRIP,logged,ftp->username,"proceed.");
		DebugLog("LOGIN: User %s (%ld) successfully connected.",ftp->username,ftp->usernumber);
		}
	FAMEStrCopy("/",ftp->cd,255);			// Set default current directory to "/"
	ftp->type = ASCII_TYPE;						// Set default transfer mode to ASCII
	fstats->UserNumber = ftp->usernumber;
	HandleStats("Successfully logged in",ftp->username);
	}

/************************************************************************************************
 * FUNCTION: pport()
 *  PURPOSE: Handles FTP command PORT
 *    INPUT: ftp => Our Master FTP Structure
 *           arg => Port Args (IP + Port)
 *   RETURN: 0 = Success, -1 = Illegal Syntax in Port Command
 ************************************************************************************************/

static int pport(struct FTP *ftp,char *arg)
	{
	long n;
	long i;

	n = 0;
	for(i=0;i<4;i++)
		{
		n = FAMEAtol(arg) + (n << 8);
		if((arg = FAMEStrChr(arg,',')) == NULL) return -1;
		arg++;
		}
	ftp->port.sin_addr.s_addr = n;
	n = FAMEAtol(arg);
	if((arg = FAMEStrChr(arg,',')) == NULL)	return -1;
	arg++;
	n = FAMEAtol(arg) + (n << 8);
	if(!n) return -1;
	ftp->port.sin_port = n;
	if(ftp->pdata!=-1)
		{
		CloseSocket(ftp->pdata);
		ftp->pdata = -1;
		}
	return 0;
	}

/*************************************************************************************************
 * FUNCTION: Passive()
 *  PURPOSE: Toggles Passive FTP Mode (PASV)
 *    INPUT: *ftp => Master FTP structure
 *   RETURN: -
 *     NOTE: Result is stored in ftp->pdata & ftp->pasv!
 *************************************************************************************************/

static void Passive(struct FTP *ftp)
	{
	LONG 			len,err;
	register 	char *p,*a;
	ULONG 		nataddress = 0UL;

	if(ftp->pdata != -1)		// For safety, as it may be possible to call it twice!
		{
		CloseSocket(ftp->pdata);
		ftp->pdata = -1;
		}
	ftp->pdata = socket(AF_INET, SOCK_STREAM, 0);
	if (ftp->pdata < 0)
		{
		usprintf(ftp->control,CRLF_NOSTRIP,nopasv,1);
		return;
		}
  setsockopt(ftp->pdata, SOL_SOCKET, SO_RCVBUF, (char *)&rcvbufsz, sizeof(rcvbufsz) );
	ftp->pasv = ftp->ctrl;
	if(ActivePortCounter!=0)												// We must choose the port from our range
		{
		ftp->pasv.sin_port = ActivePortCounter;				// Use our own port counter
		ActivePortCounter++;
		if(ActivePortCounter >= fconfig->PortHi) ActivePortCounter = fconfig->PortLo;
		}
	else
		{
		ftp->pasv.sin_port = 0;												// Let the stack decide
		}
	if (bind(ftp->pdata, (struct sockaddr *) &ftp->pasv, sizeof(ftp->pasv)) >= 0)
		{
		len = sizeof(ftp->pasv);
		if (getsockname(ftp->pdata, (struct sockaddr *) &ftp->pasv, &len) >= 0)
			{
			if (listen(ftp->pdata, 1) >= 0)
				{
				a = (char *) &ftp->pasv.sin_addr;
				p = (char *) &ftp->pasv.sin_port;
				if(*PassiveIP)
					{
					nataddress = inet_addr(PassiveIP);
					a = (char *) &nataddress;
					}
				usprintf(ftp->control,CRLF_NOSTRIP,pasvcon,UC(a[0]),UC(a[1]), UC(a[2]), UC(a[3]), UC(p[0]), UC(p[1]));
				return;
				}
			else err = 4;
			}
		else err = 3;
		}
	else err = 2;
	DebugLog("Passive(): TCP Error %ld (%s)",Errno(),tcp_error(Errno()));
	CloseSocket(ftp->pdata);
	ftp->pdata = -1;
	usprintf(ftp->control,CRLF_NOSTRIP,nopasv,err);
	}

/************************************************************************************************
 * FUNCTION: ChangeDirectory()
 *  PURPOSE: Handles FTP command CWD/XCWD
 *    INPUT: *ftp => Pointer to Master FTP structure
 *           *arg => Argument to which we have to change dir
 *   RETURN: -
 ************************************************************************************************/

static void ChangeDirectory(struct FTP *ftp, char *arg)
	{
	struct 	FConf *f = fconf1;

	if(!Stricmp(arg,"..") || !Stricmp(arg,"/"))
		{
    FAMEStrCopy("/",ftp->cd,255);
		usprintf(ftp->control,CRLF_NOSTRIP,cwdmsg,"/");
		fstats->CurrentConf = 0;
		HandleStats("Joined root directory");
		return;
		}
	if(*arg=='/') arg++;
	while(f)
    {
		if(!Strnicmp(f->ConfName,arg,strlen(f->ConfName)))
			{
			FAMEStrCopy(f->ConfName,ftp->cd,255);
			FAMEStrCat("/",ftp->cd);
			usprintf(ftp->control,CRLF_NOSTRIP,cwdmsg,ftp->cd);
			fstats->CurrentConf = f->ConfNumber;
			HandleStats("Joined conf %s", f->ConfName);
			return;
			}
		f=f->next;
		}
	usprintf(ftp->control,CRLF_NOSTRIP,nodir,arg);
	fstats->CurrentConf = 0;
	}

/************************************************************************************************
 * FUNCTION: FTPList()
 *  PURPOSE: Handles FTP command LIST including Parameters
 *    INPUT: *ftp => Master FTP structure
 *           *arg => The Path to list
 ************************************************************************************************/

static void FTPList(struct FTP *ftp, char *arg)
	{
	BPTR		fh;
  long		total,datcon=NULL,rc;
	char    myfname[256],path[256];

	SPrintf(myfname,"T:FAME-FTPd.%08lx",FindTask(NULL));
  if(!(fh = Open(myfname,MODE_READWRITE)))
		{
		usprintf(ftp->control,CRLF_NOSTRIP,noconn);
		return;
		}
	if(*arg=='\0') FAMEStrCopy(ftp->cd,path,255);
	else
		{
		if(!Stricmp(arg,"-l") || !Stricmp(arg,"-la") || !Stricmp(arg,"-al")) FAMEStrCopy(ftp->cd,path,255);
		else FAMEStrCopy(arg,path,255);
		}
	if(!Stricmp(path,"..")) FAMEStrCopy("/",path,255);		// We have only one level of directory, so up means always root
	if(ShowDirContents(path,fh,ftp))
		{
    Close(fh);
		DeleteFile(myfname);
		usprintf(ftp->control,CRLF_NOSTRIP,nodir,path);
		DebugLog("ERROR: List Dir failed: %s",path);
		return;
    }
	Flush(fh);
	rc = DataSocket(ftp,path,0);
  switch(rc)
		{
		case	0:	datcon = ftp->pdata;
							break;
		case	1:	datcon = ftp->data;
							break;
		case	-1:	Close(fh);
							DeleteFile(myfname);
							return;
		}
	Seek(fh,0L,OFFSET_BEGINNING);
	total = sendfile(fh,datcon,ftp->type,FALSE);
	if(total == -1)		/* An error occurred on the data connection */
		{
		DebugLog("FTPList/Sendfile: %ld",total);
		usprintf(ftp->control,CRLF_NOSTRIP,noconn);
		}
	else 	usprintf(ftp->control,CRLF_NOSTRIP,txok, total);
	Close(fh);
	CloseSocket(datcon);
	if(!rc) ftp->pdata = -1;
	else ftp->data = -1;
	DeleteFile(myfname);
	}

/**************************************************************************************************
 * FUNCTION: ShowDirContents()
 *  PURPOSE: Displays directory contents for a given conference
 *    INPUT: arg => Path to show
 *           fh  => Filehandle where to print the dir
 *   RETURN: 0 = Success | 1 = Failure
 **************************************************************************************************/

static long ShowDirContents(char *arg, BPTR fh, struct FTP *ftp)
	{
	struct	FConf *f;
	char		datebuf[LEN_DATSTRING*2];
	struct	FTPUploads *f1;

	FormatStamp(NULL,datebuf,myloc,TRUE);
	f=fconf1;
	if(!Stricmp(arg,"/"))		// Rootdir requested, dump out all available conferences
		{
		while(f)
			{
			FPrintf(fh,dirfmt,'d',1,FameInfo->SysOp,0,datebuf,f->ConfName);
			f=f->next;
			}
		return(0);
		}

	// Other directory requested, check if we can find the desired one

	while(f)
		{
   	if(!Strnicmp(f->ConfName,arg,strlen(f->ConfName)))
			{
			if(*fconfig->HowToUploadFile!='\0')
				{
				FPrintf(fh,dirfmt,'-',1,FameInfo->SysOp,GetFileSize(fconfig->HowToUploadFile),datebuf,FilePart(fconfig->HowToUploadFile));
				}

  // Now list all uploaded files for this conference

  		for(f1=(struct FTPUploads *)filelist.mlh_Head;f1!=(struct FTPUploads *)&filelist.mlh_Tail;f1=(struct FTPUploads *)f1->Node.mln_Succ)
 				{
				if(f1->ConfNum == f->ConfNumber)
					{
					FPrintf(fh,dirfmt,'-',1,ftp->username,f1->FileSize,f1->FileDate,f1->FileName);
					}
				}
			return(0);
			}
		f=f->next;
		}
	return(1);
	}

/**************************************************************************************************
 * FUNCTION: DataSocket()
 *  PURPOSE: Returns the actual data socket to use:
 *    INPUT: ftp  => Master FTP structure
 *           path => Path to show in control message
 *           mode => TRUE = Filesize will be added | FALSE = Only filename or path is shown
 *   RETURN:  0 => Use ftp->pdata connection
 *            1 => Use ftp->data connection
 *           -1 => Error
 **************************************************************************************************/

long DataSocket(struct FTP *ftp,char *path, short mode)
	{
	struct 	sockaddr_in dport;
	long		fsize;
	long		retry = 0;

	if(ftp->pdata >=0)
		{
		struct sockaddr_in from;
    LONG s, fromlen = sizeof(from);

    FAMEFillMem(&from,0,sizeof(struct sockaddr_in));
		s = accept(ftp->pdata,(struct sockaddr *) &from, &fromlen);
		if(s < 0)
			{
			usprintf(ftp->control,CRLF_NOSTRIP,noopendata);
			CloseSocket(ftp->pdata);
			ftp->pdata = -1;
			return(-1);
      }
		if(mode)
			{
      fsize = GetFileSize(path);
			usprintf(ftp->control,CRLF_NOSTRIP,filesend,ftp->type == ASCII_TYPE ? "ASCII" : "BINARY",FilePart(path),fsize);
      }
		else
			{
			usprintf(ftp->control,CRLF_NOSTRIP,sending,ftp->type == ASCII_TYPE ? "ASCII" : "BINARY",path);
			}
		CloseSocket(ftp->pdata);
		ftp->pdata = s;
		return(0);
		}

  if(ftp->data >=0)
		{
    usprintf(ftp->control,CRLF_NOSTRIP,"125 Using existing data connection for %s.\r\n",path);
		return(1);
		}

	if(mode)
		{
    fsize = GetFileSize(path);
		usprintf(ftp->control,CRLF_NOSTRIP,filesend,ftp->type == ASCII_TYPE ? "ASCII" : "BINARY",FilePart(path),fsize);
		}
	else
		{
		usprintf(ftp->control,CRLF_NOSTRIP,sending,ftp->type == ASCII_TYPE ? "ASCII" : "BINARY",path);
		}
	FAMEFillMem(&dport,0,sizeof(struct sockaddr_in));
	ftp->data = socket(AF_INET,SOCK_STREAM,0);
  setsockopt(ftp->data, SOL_SOCKET, SO_RCVBUF, (char *)&rcvbufsz, sizeof(rcvbufsz) );
	dport.sin_family			=	AF_INET;
	dport.sin_addr.s_addr	=	ftp->port.sin_addr.s_addr;
	dport.sin_port				=	ftp->port.sin_port;
  while(connect(ftp->data,(struct sockaddr *)&ftp->port,sizeof(struct sockaddr_in)) < 0)
		{
		if(Errno()==EADDRINUSE || errno == EINTR && retry < CONNECT_RETRY)
			{
			DebugLog("DataSocket(): connect() failed in retry %ld with code %ld.",retry,Errno());
			retry++;
			Delay(CONNECT_WAIT);
      continue;
  		}
		DebugLog("DataSocket->connect(): TCP Error %ld (%s)",Errno(),tcp_error(Errno()));
		CloseSocket(ftp->data);
		ftp->data = -1;
		usprintf(ftp->control,CRLF_NOSTRIP,noopendata);
		return(-1);
		}
	return(1);
	}

/**************************************************************************************************
 * FUNCTION: sendfile()
 *  PURPOSE: Send a file (opened by caller) on a network socket
 *    INPUT:   fp 		=> Filepointer to already opened file
 *              s 		=> Socket to send file to
 *           mode 		=> Transfer Mode (ASCII/BINARY)
 *           sendCRLF => TRUE = Send CRLF combination with buffered check, FALSE = ignore CRLF sending
 *   RETURN:   -1 		=> Error occured
 *           <>-1 		=> Amount of bytes sent
 **************************************************************************************************/

long sendfile(BPTR fp,LONG s,int mode, BOOL sendCRLF)
	{
	long 	total = 0;
	long 	cnt;
	char	*asciibuf;

	if(!GetFileSizeFH(fp)) return(0);
	switch(mode)
		{
		default:
		case LOGICAL_TYPE:
		case IMAGE_TYPE:		for(;;)
													{
													SetIoErr(0);
													cnt = Read(fp,filebuf,BLKSIZE);
													if(!cnt)
														{
                            cnt=IoErr();
														if(cnt) DebugLog("sendfile(): IoErr() => %ld - Total => %ld!",cnt,total);
														break;
														}
													total += cnt;
													if(send(s,filebuf,cnt,0) == -1) return(-1);
													}
												break;

		case ASCII_TYPE:    if(!(asciibuf = AllocPooled(mem_pool, BLKSIZE))) return(-1);
												while(1)
													{
													if(!FGets(fp,asciibuf,BLKSIZE))
														{
														cnt = IoErr();
                            if(cnt)
															{
															DebugLog("sendfile(): IoErr() => %ld - Total => %ld!",cnt,total);
                              return(-1);
                              }
														else 
															{
															break;
															}
                            }
													if(sendCRLF==TRUE)
														{
	                          CutCRLF(asciibuf);
														FAMEStrCat("\n\r",asciibuf);
														}
            							cnt = send(s,asciibuf,strlen(asciibuf),0);
													if(cnt==-1) return(-1);
                          total +=cnt;
													}
												FreePooled(mem_pool,asciibuf,BLKSIZE);
												break;

		}
	return(total);
	}

/**************************************************************************************************
 * FUNCTION: recvfile()
 *  PURPOSE: Recieve a file (opened by caller) from a network socket
 *    INPUT:   fp => Filepointer to already opened and writeable file
 *              s => Socket to recieve file from
 *           mode => Transfer Mode (ASCII/BINARY)
 *   RETURN:   -1 => Error occured
 *           <>-1 => Amount of bytes recieved
 **************************************************************************************************/

long recvfile(BPTR fp,LONG s,int mode)
	{
	int 	cnt;
	long	total=0;
	UBYTE	ascbuf[2];

	switch(mode)
		{
		default:
		case LOGICAL_TYPE:
		case IMAGE_TYPE:		for(;;)
													{
													FAMEFillMem(filebuf,0,BLKSIZE);
                          cnt = recv(s,filebuf,BLKSIZE,0);
                          if(cnt==-1)
														{
                            return(-1);
														}
                          if(!cnt) break;
													total+=cnt;
													if(Write(fp,filebuf,cnt)!=cnt)
														{
														if(IoErr())
															{
															DebugLog("Binary-Write() failed with IOErr=%ld",IoErr());
															return(-1);
                            	}
                            else break;
														}
													}
												break;

		case ASCII_TYPE:		for(;;)
													{
                          cnt = recv(s,&ascbuf[0],1,0);
													ascbuf[1] = '\0';
                          if(!cnt) break;
													if(cnt==-1) return(-1);
													total+=cnt;
                          if(FPutC(fp,(ULONG)ascbuf[0])==ENDSTREAMCH)
														{
														return(-1);
                           	}
                          }
												break;

		}
  return(total);
  }

/**************************************************************************************************
 * FUNCTION: HandleDownload()
 *  PURPOSE: Allows to send specific files to the client (info pages)
 *    INPUT: *ftp => Master FTP structure
 *           *arg => Argument (Filename) to retrieve
 *   RETURN: -
 **************************************************************************************************/

static void HandleDownload(struct FTP *ftp, char *arg)
	{
  long		datcon=NULL,rc,total;
	BPTR		fh;

	if(*fconfig->HowToUploadFile=='\0')
		{
    usprintf(ftp->control,CRLF_NOSTRIP,noperm);
		return;
		}
	if(!Stricmp(ftp->cd,"/"))
		{
		usprintf(ftp->control,CRLF_NOSTRIP,noperm);
		return;
		}
	if(Stricmp(FilePart(arg),FilePart(fconfig->HowToUploadFile)))
		{
    usprintf(ftp->control,CRLF_NOSTRIP,noperm);
		return;
		}
	rc = DataSocket(ftp,fconfig->HowToUploadFile,1);
  switch(rc)
		{
		case	0:	datcon = ftp->pdata;
							break;
		case	1:	datcon = ftp->data;
							break;
		case	-1: return;
		}
	if(!(fh = Open(fconfig->HowToUploadFile,MODE_OLDFILE)))
		{
		usprintf(ftp->control,CRLF_NOSTRIP,noconn);
		total = -1;
		}
	else
		{
		HandleStats("DL: %s",FilePart(arg));
		total = sendfile(fh,datcon,ftp->type,TRUE);
		Close(fh);
		}
	if(total == -1)
		{
		/* An error occurred on the data connection */
		usprintf(ftp->control,CRLF_NOSTRIP,noconn);
		shutdown(datcon,2);	/* Blow away data connection */
		CloseSocket(datcon);
		return;
		}
	else usprintf(ftp->control,CRLF_NOSTRIP,txok,total);
	shutdown(datcon,1);
	CloseSocket(datcon);
	if(!rc) ftp->pdata = -1;
	else ftp->data = -1;
	FAMEAdd64(0UL,total,&FameInfo->FileDLBytesHi);
	FameInfo->FilesDL++;
	HandleStats("Finished downloading %ld bytes",total);
	}

/**************************************************************************************************
 * FUNCTION: HandleUpload()
 *  PURPOSE: Retrieves one file from the client and adds the file to conferences playpen dir
 *    INPUT: *ftp => Master FTP Structure
 *           *arg => Filename in Question
 *   RETURN: -
 **************************************************************************************************/

static void HandleUpload(struct FTP *ftp, char *arg)
	{
  long		datcon=NULL,rc,total,mycnum;
	BPTR		fh;
	char		fbuf[384],fname[32];

	if(CheckIfOnline(ftp->usernumber)==TRUE)
		{
    usprintf(ftp->control,CRLF_NOSTRIP,onlinewarn);
		return;
		}
  if(!(ftp->UserFlags & UF_UPLOAD))
		{
		usprintf(ftp->control,CRLF_NOSTRIP,noperm);
		return;
		}
	if(!Stricmp(ftp->cd,"/"))
		{
		usprintf(ftp->control,CRLF_NOSTRIP,noperm);
		return;
		}
	if(*arg=='\0')	// Upload without Filename is really not possible
		{
		usprintf(ftp->control,CRLF_NOSTRIP,unsupp);
		return;
    }
	if(strlen(FilePart(arg))>fconfig->FileNameLength)
		{
		usprintf(ftp->control,CRLF_NOSTRIP,fnametoolong,fconfig->FileNameLength);
		return;
		}
	mycnum = GetConfNumber(ftp->cd);
	if(!mycnum)
		{
    DebugLog("ERROR: Got %ld as Confnumber for %s?",mycnum,ftp->cd);
		usprintf(ftp->control,CRLF_NOSTRIP,fileerror);
		return;
		}
	if(CheckForDoubles(arg,mycnum)==TRUE)							// Filename already exists, abort upload
		{
		usprintf(ftp->control,CRLF_NOSTRIP,fileexists);
		return;
		}
	FAMEStrCopy(fconfig->UploadTempPath,fbuf,255);
	SPrintf(fname,"UL_CONF%ld_%08lx",mycnum,FindTask(NULL));
  AddPart(fbuf,fname,383);
	rc = DataSocket(ftp,arg,0);
  switch(rc)
		{
		case	0:	datcon = ftp->pdata;
							break;
		case	1:	datcon = ftp->data;
							break;
		case	-1: return;
		}
	if(!(fh = Open(fbuf,MODE_READWRITE)))
		{
    char ioerr[80];
		Fault(IoErr(),NULL,ioerr,79);
		DebugLog("UL FAILURE FOR FILE: %s (%s)",fbuf,ioerr);
    usprintf(ftp->control,CRLF_NOSTRIP,cantmake,arg,ioerr);
    total = -1;
		}
  else
		{
		fstats->CurrentConf = mycnum;	// Set actual conferencenr, so that it will be written together with stats
		HandleStats("UL: %s",FilePart(arg));
		total = recvfile(fh,datcon,ftp->type);
		Close(fh);
		}
	if(total == -1)				/* An error occurred on the data connection */
		{
		shutdown(datcon,2);	/* Blow away data connection */
		CloseSocket(datcon);
    DeleteFile(fbuf);
		usprintf(ftp->control,CRLF_NOSTRIP,noconn);
		return;
		}
	shutdown(datcon,0);
	CloseSocket(datcon);
	if(!rc) ftp->pdata = -1;
	else ftp->data = -1;

	/*  Add the uploaded file to our ul list so we can add them l8er to the filelist
	 *  Also the file will be moved to the conference playpen directory including
   *  Description file, so that the user can add the files l8er
   */

	if(AddFileToUList(arg,GetFileSize(fbuf),mycnum,ftp,fbuf)==TRUE)
		{
		usprintf(ftp->control,CRLF_NOSTRIP,rxok,total);
		FAMEAdd64(0UL,total,&FameInfo->FileULBytesHi);
		FameInfo->FilesUL++;
		fstats->ULFiles++;
		FAMEAdd64(0UL,total,&fstats->ULBytesHi);
		HandleStats("Finished Uploading %ld bytes",total);
		}
	else
		{
		usprintf(ftp->control,CRLF_NOSTRIP,fileerror);
		}
	DeleteFile(fbuf);
	}

/**************************************************************************************************
 * FUNCTION: SendTransferStats()
 *  PURPOSE: Sends actual stats to given socket. Uses struct FameInfo data
 *    INPUT: Socket where to send the data or -1 to add only to Log
 *   RETURN: -
 **************************************************************************************************/

static void SendTransferStats(long s)
	{
	char	ful[40],dul[40];
	char byestatsstr[]= "221- \r\n221- Transfered bytes: UL: %12s Bytes | DL: %12s Bytes\r\n221- Transfered files: UL: %12ld Files | DL: %12ld Files\r\n";

	FAMENum64ToStr(FameInfo->FileULBytesHi,FameInfo->FileULBytesLo,FNSF_GROUPING|FNSF_NUMLOCALE,39,ful);
	FAMENum64ToStr(FameInfo->FileDLBytesHi,FameInfo->FileDLBytesLo,FNSF_GROUPING|FNSF_NUMLOCALE,39,dul);
  if(s >=0)
		{
		usprintf(s,CRLF_NOSTRIP,byestatsstr,ful,dul,FameInfo->FilesUL,FameInfo->FilesDL);
		}
	DebugLog("Stats: UL: %s Bytes / %ld Files | DL: %s Bytes / %ld Files",ful,FameInfo->FilesUL,dul,FameInfo->FilesDL);
	}

/**************************************************************************************************
 * FUNCTION: HandleSize()
 *  PURPOSE: Sends filesize of given file to given socket. (FTP command SIZE <file>)
 *    INPUT: *ftp => Master FTP Structure
 *           *arg => Filename in Question
 *   RETURN: -
 **************************************************************************************************/

static void HandleSize(struct FTP *ftp, char *arg)
	{
	struct	FTPUploads *f1;
	long		mycnum;

	if(!Stricmp(ftp->cd,"/"))
		{
		usprintf(ftp->control,CRLF_NOSTRIP,noperm);
    return;
		}
	if(!Stricmp(FilePart(arg),FilePart(fconfig->HowToUploadFile)))
		{
		usprintf(ftp->control,CRLF_NOSTRIP,size,GetFileSize(fconfig->HowToUploadFile));
		return;
		}

	// User seems to want the size of some of his own files, so check for the file and
  // Return size if file can be found, else return file not found error.

	mycnum = GetConfNumber(ftp->cd);
	for(f1=(struct FTPUploads *)filelist.mlh_Head;f1!=(struct FTPUploads *)&filelist.mlh_Tail;f1=(struct FTPUploads *)f1->Node.mln_Succ)
		{
		if(f1->ConfNum == mycnum && !Stricmp(f1->FileName,FilePart(arg)))
			{
			usprintf(ftp->control,CRLF_NOSTRIP,size,f1->FileSize);
			return;
			}
		}
	usprintf(ftp->control,CRLF_NOSTRIP,filenotfound);
	}

/**************************************************************************************************
 * FUNCTION: HandleFeatures()
 *  PURPOSE: Returns supported Features of this server according to RFC 2389
 *    INPUT: *ftp => Master FTP Structure
 *   RETURN: -
 **************************************************************************************************/

static void HandleFeatures(struct FTP *ftp)
	{
	usprintf(ftp->control,CRLF_NOSTRIP,"211- Extensions supported:\r\n SIZE\r\n CLNT\r\n211 End\r\n");
  }

/**************************************************************************************************
 * FUNCTION: HandleClient()
 *  PURPOSE: Takes Client name and version, concatenates them and copy them to fconfig->ClientVersion
 *    INPUT: *ftp => Master FTP Structure
 *           *arg => Client Program Info
 *   RETURN: -
 **************************************************************************************************/

static void HandleClient(struct FTP *ftp, char *arg)
	{
	if(*arg)
		{
		FAMEStrCopy(arg,ftp->ClientVersion,128);
		}
	usprintf(ftp->control,CRLF_NOSTRIP,okay);
	DebugLog("Remote Client is '%s'",ftp->ClientVersion);
	}

/**************************************************************************************************
 * FUNCTION: HandleHelp()
 *  PURPOSE: Returns either list of all supported commands or try to give help for a specific command
 *    INPUT: *ftp => Master FTP Structure
 *           *arg => Command in question
 *   RETURN: -
 **************************************************************************************************/

static void HandleHelp(struct FTP *ftp, char *arg)
	{
	char	**lv;
  char  helpbuf[256],dummy[12];
	int		cnt = 0;

	if(*arg)		// User wants help for specific command
		{
		for(lv = commands; *lv != NULL; lv++)
			{
			if(!Stricmp(arg,*lv))
				{
				usprintf(ftp->control,CRLF_NOSTRIP,"214 Syntax: %s\r\n",cmdhelp[cnt]);
				return;
        }
			cnt++;
			}
		usprintf(ftp->control,CRLF_NOSTRIP,unimp);
		return;
    }

	// User wants complete list of supported commands

	usprintf(ftp->control,CRLF_NOSTRIP,"214- The following commands are recognized:\r\n");
	FAMEStrCopy("   ",helpbuf,255);
	for(lv = commands; *lv != NULL; lv++)
		{
		if(!(cnt % 6) && cnt > 0)
			{
			FAMEStrCat("\r\n",helpbuf);
			usprintf(ftp->control,CRLF_NOSTRIP,helpbuf);
			FAMEFillMem(helpbuf,0,255);
			FAMEStrCopy("   ",helpbuf,255);
			}
		SPrintf(dummy,"%-9s",FAMEStrToUpper(*lv));
		FAMEStrCat(dummy,helpbuf);
		cnt++;
		}
	if(*helpbuf)	usprintf(ftp->control,CRLF_NOSTRIP,"%s\r\n",helpbuf);
	usprintf(ftp->control,CRLF_NOSTRIP,"214 Direct comments to siegel@trsi.org (http://www.saschapfalz.de).\r\n");
	}

/**************************************************************************************************
 * FUNCTION: HandleStat()
 *  PURPOSE: Handles FTP command STAT
 *    INPUT: ftp => Master FTP structure
 *           arg => Optional filename (not supported)
 *   RETURN: -
 **************************************************************************************************/

static  void	HandleStat(struct FTP *ftp, char *arg)
	{
	char	ful[40],dul[40];

	if(*arg)
		{
		usprintf(ftp->control,CRLF_NOSTRIP,unimp);
		return;
		}
	FAMENum64ToStr(FameInfo->FileULBytesHi,FameInfo->FileULBytesLo,FNSF_GROUPING|FNSF_NUMLOCALE,39,ful);
	FAMENum64ToStr(FameInfo->FileDLBytesHi,FameInfo->FileDLBytesLo,FNSF_GROUPING|FNSF_NUMLOCALE,39,dul);
	usprintf(ftp->control,CRLF_NOSTRIP,"211- Stats:\r\n211- \r\n");
	usprintf(ftp->control,CRLF_NOSTRIP,"211-   Username: %s\r\n",ftp->username);
	usprintf(ftp->control,CRLF_NOSTRIP,"211- Conference: %s\r\n",ftp->cd);
	usprintf(ftp->control,CRLF_NOSTRIP,"211-    Last on: %s (%s)\r\n",fstats->ConnectTime,ftp->IPaddr);
	usprintf(ftp->control,CRLF_NOSTRIP,"211-    Timeout: %ld seconds\r\n",fconfig->Timeout);
	usprintf(ftp->control,CRLF_NOSTRIP,"211-   Bytes UL: %s Bytes\r\n",ful);
	usprintf(ftp->control,CRLF_NOSTRIP,"211-   Bytes DL: %s Bytes\r\n",dul);
	usprintf(ftp->control,CRLF_NOSTRIP,"211-   Files UL: %ld Files\r\n",FameInfo->FilesUL);
	usprintf(ftp->control,CRLF_NOSTRIP,"211-   Files DL: %ld Files\r\n",FameInfo->FilesDL);
	usprintf(ftp->control,CRLF_NOSTRIP,"211 \r\n");
	}

/**************************************************************************************************
 * FUNCTION: CloseLibs()
 *  PURPOSE: Frees all resources and closes all open libraries.
 *    INPUT: -
 *   RETURN: -
 *     NOTE: Program will be exited when this function is called!
 **************************************************************************************************/

void CloseLibs(void)
	{
	if(server_socket>=0)
		{
		shutdown(server_socket,2);
		CloseSocket(server_socket);
		server_socket = -1;
   	}
	if(confh!=-1 && confh!=NULL) 	Close(confh);
	FreeConferenceList();
	if(fib)						FreeDosObject(DOS_FIB,fib); fib = NULL;
	if(mem_pool)			DeletePool(mem_pool); mem_pool = NULL;
  if(SocketBase)		CloseLibrary(SocketBase);	SocketBase = NULL;
	if(UtilityBase)		CloseLibrary(UtilityBase); UtilityBase = NULL;
  if(LocaleBase)
		{
		if(myloc) CloseLocale(myloc);myloc=NULL;
		CloseLibrary((struct Library *)LocaleBase);	LocaleBase = NULL;
		};
	if (FAMEBase) 		CloseLibrary((struct Library *)FAMEBase);	FAMEBase = NULL;
	if(SysLogBase)  	CloseLibrary(SysLogBase); SysLogBase = NULL;
	if(IntuitionBase)	CloseLibrary((struct Library *)IntuitionBase); IntuitionBase = NULL;
	exit(0);
	}

/**************************************************************************************************
 * FUNCTION: fail()
 *  PURPOSE: Prints an Error code via EasyRequest and also to the Debug.log file
 *    INPUT: iocode 		=> Optional DOS error code if > 0
 *           errstring  => printf() style format string
 *                 ...  => Optional arguments for format string
 *   RETURN: -
 **************************************************************************************************/

void fail(long iocode, char *errstring, ...)
	{
	char	errbuf[384],ioerr[80];

	RawDoFmt(errstring, (long *)(&errstring + 1), (void (*))"\x16\xc0\x4e\x75",errbuf);
	DebugLog("ERROR: %s",errbuf);
	if(iocode)
		{
		Fault(iocode,"\n\nDOS-ERROR",ioerr,79);
		strcat(errbuf,ioerr);
		}
	if(IntuitionBase)
		{
		struct EasyStruct MyES=
			{
			sizeof(struct EasyStruct),
			0,
			NULL,
      "%s",
      "Okay",
			};
   	EasyRequest(NULL,&MyES,NULL,errbuf);
		}
	}

/**************************************************************************************************
 * FUNCTION: InitStats()
 *  PURPOSE: Initializes the Stats structure (Things that do not change anymore)
 *    INPUT: ftp => Master FTP structure
 *   RETURN: -
 *    NOTES: Uses global structure fstats
 **************************************************************************************************/

static void InitStats(struct FTP *ftp)
	{
	SPrintf(fstats->TaskAddress,"%08lx",FindTask(NULL));
	fstats->UserNumber = ftp->usernumber;
	FormatStamp(NULL,fstats->ConnectTime,NULL,FALSE);
  }

/**************************************************************************************************
 * FUNCTION: HandleStats()
 *  PURPOSE: Manages the stats file stored in T:
 *    INPUT: actionstring	=> What the server is currently doing
 *   RETURN: TRUE = Success, FALSE = Failure
 **************************************************************************************************/

static BOOL HandleStats(char *actionstring, ...)
	{
	int     cnt = 0,entries,i, found = -1;
	BPTR		fp;

	FAMEFillMem(tmpptr,0,sizeof(struct FTPdStats));
	if(&actionstring+1) RawDoFmt(actionstring,(long *)(&actionstring + 1), (void (*))"\x16\xc0\x4e\x75",fstats->ActionString);
	else FAMEStrCopy(actionstring,fstats->ActionString,79);
  while((fp = Lock(lockfile,EXCLUSIVE_LOCK)))		// Respect the lockfile, it is the important thing!
		{
		UnLock(fp);
		Delay(STATS_RETRY_DELAY);
    }
	while(cnt < STATS_FILE_RETRY)
		{
		if(!(fp = Open(statname,MODE_READWRITE)))		// File is accessed by many instances!
			{
			cnt++;
			Delay(STATS_RETRY_DELAY);
			continue;
			}
		entries = GetFileSizeFH(fp) / sizeof(struct FTPdStats);
		if(entries)
			{
      for(i = 0; i < entries; i++)
				{
				Seek(fp,i * sizeof(struct FTPdStats),OFFSET_BEGINNING);
				if(!Read(fp,tmpptr,sizeof(struct FTPdStats)))
					{
					DebugLog("Reading stats error: %ld",IoErr());
          Close(fp);
					return(FALSE);
					}
				if(!Stricmp(tmpptr->TaskAddress,fstats->TaskAddress))
					{
					found = i;
					break;
					}
				}
			}
		if(found!=-1)
			{
      Seek(fp, found * sizeof(struct FTPdStats), OFFSET_BEGINNING);
			}
		else
			{
      Seek(fp,0L,OFFSET_END);
			}
    if(Write(fp,fstats,sizeof(struct FTPdStats))!=sizeof(struct FTPdStats))
			{
      DebugLog("Writing stats error: %ld",IoErr());
			}
		Close(fp);
		return(TRUE);
		}
	DebugLog("Error while trying to open %s: %ld",statname,IoErr());
	return(FALSE);
	}

/**************************************************************************************************
 * FUNCTION: RemoveFromStats()
 *  PURPOSE: Removes our own entry from stats list
 *    INPUT: ftp => Master FTP structure
 *   RETURN: TRUE = Success, FALSE = Failure
 **************************************************************************************************/

static BOOL RemoveFromStats(void)
	{
	int   cnt = 0,entries,i,retries;
	BPTR	fp,fp2,fp3;
  char  tmpname[32];

	/*  First we have to make sure that we are alone here, so create lockfile, and wait for it
   *  to be created until we can proceed. No other instance writes then to stats until lockfile
   *  is removed again.
   */

	retries = 0;
  while(1)
		{
    if(retries > 255) 
			{
			fail(NULL,"Cannot remove ourself from the statsfile, inform SieGeL!!!");
			return(FALSE);
			}
		fp3 = Lock(lockfile,SHARED_LOCK);
		if(fp3)	/* File exists already, someone else is currently writing */
			{
			UnLock(fp3);
			Delay(STATS_RETRY_DELAY);
			retries++;
			continue;
      }
		if(!(fp3 = Open(lockfile,MODE_NEWFILE)))	/* File cannot be written exclusivly, so another one was faster, wait */
			{
			Delay(STATS_RETRY_DELAY);
			retries++;
			continue;
			}
		Close(fp3);
		break;
		}
	FAMEFillMem(tmpptr,0,sizeof(struct FTPdStats));
	SPrintf(tmpname,"T:lst.%s",fstats->TaskAddress);
	while(cnt < STATS_FILE_RETRY)
		{
		if(!(fp = Open(statname,MODE_READWRITE)))		// File is accessed by many instances!
			{
			cnt++;
			Delay(STATS_RETRY_DELAY);
			continue;
			}
		entries = GetFileSizeFH(fp) / sizeof(struct FTPdStats);
		if(entries)
			{
			if(!(fp2 = Open(tmpname,MODE_NEWFILE)))
				{
        DebugLog("Cannot create tmp file %s (%ld)?",tmpname,IoErr());
        Close(fp);
				DeleteFile(lockfile);
				return(FALSE);
				}
      for(i = 0; i < entries; i++)
				{
				Seek(fp,i * sizeof(struct FTPdStats),OFFSET_BEGINNING);
				if(!Read(fp,tmpptr,sizeof(struct FTPdStats)))
					{
					DebugLog("Reading stats error: %ld",IoErr());
          Close(fp);Close(fp2);
          DeleteFile(tmpname);
					DeleteFile(lockfile);
					return(FALSE);
					}
				if(!Stricmp(tmpptr->TaskAddress,fstats->TaskAddress))	// Our record, leave it out and continue
					{
          continue;
					}
				if(Write(fp2,tmpptr,sizeof(struct FTPdStats))!=sizeof(struct FTPdStats))
					{
					DebugLog("Cannot write to tmp file %s (%ld) ?",tmpname,IoErr());
          Close(fp);Close(fp2);
					DeleteFile(tmpname);
					DeleteFile(lockfile);
					return(FALSE);
					}
				}
			Close(fp2);
			}
    Close(fp);
    if(FAMEDosMove(tmpname,statname,fconfig->FileBufferSize*1024,NULL)==FALSE) DebugLog("Stats-DosMove() failed: %ld",IoErr());
		DeleteFile(lockfile);
		return(TRUE);
		}
	DebugLog("Error while trying to open %s: %ld",statname,IoErr());
	DeleteFile(lockfile);
	return(FALSE);
	}

/**************************************************************************************************
 * FUNCTION: CountOnlineUser()
 *  PURPOSE: Counts how many users are currently logged in
 *    INPUT: -
 *   RETURN: User Count or -1 in case of an error
 **************************************************************************************************/

static long CountOnlineUsers(void)
	{
	BPTR	fp;
	long	cnt = 0,myentries;

	while(cnt < STATS_FILE_RETRY)
		{
		if(!(fp = Open(statname,MODE_OLDFILE)))		// File is accessed by many instances!
			{
			if(IoErr()==ERROR_OBJECT_NOT_FOUND)			// IoErr() = 205, this is okay and not treated as error
				{
				return(0);
        }
			cnt++;
			Delay(STATS_RETRY_DELAY);
			continue;
			}
		ExamineFH(fp,fib);
		Close(fp);
		myentries = fib->fib_Size / sizeof(struct FTPdStats);
		return(myentries);
    }
	return(-1);
	}
