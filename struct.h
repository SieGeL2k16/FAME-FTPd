/****************************************************************************************
 *  PROJECT: FAME-FTPd
 *     FILE: struct.h
 *  PURPOSE: FTP related structures and global variables - main definitions for main.c
 *  CREATED: 05-MAY-2003
 * MODIFIED: 24-APR-2004
 *   AUTHOR: Sascha 'SieGeL' Pfalz
 ****************************************************************************************/

#include "global_defines.h"

struct Library 				*SocketBase 		= NULL;	/* BSDsocket library */
struct Library 				*UtilityBase 		= NULL;	/* utility library */
struct FAMELibrary		*FAMEBase 			= NULL; /* FAME library */
struct LocaleBase 		*LocaleBase 		= NULL;	/* Locale library */
struct Library 				*SysLogBase 		= NULL;	/* Optional logging to SysLogD */
struct IntuitionBase	*IntuitionBase	=	NULL; /* Required for EasyRequest() */
struct Locale 				*myloc 					= NULL; /* Locale catalog */

/*
 *  Name of stats file
 */

static	char statname[]="T:FAME-FTPd.stats";

/*
 *  server_socket is our socket comming from InetD
 */

LONG server_socket = -1;

/*
 *  As we initialize bsdsocket.library we have to supply also our own error vars
 */

int myerrno=-1,myherrno=-1;

/*
 *  Name of our Program, is used for syslog() logging purposes
 */

extern char *_ProgramName;

/*
 *  Memory pointers for general I/O:
 */

char *readbuf = NULL;				/* Command line buffer */
char *readtmp = NULL;       /* Temporary copy buffer in readline() */
char *uspbuf  = NULL;				/* Buffer used in usprintf() */
APTR filebuf 	= NULL;				/* Buffer for File I/O */


/*  Our global FAME info structure, will be filled by Obtaining() FAME semaphore
 *  and copy all required informations into this structure
 */

struct FAMEInfos
	{
	char	BBSName[256];
	char	SysOp[32];
	long	FAMEVersion,
				FAMERevision,
				NodeCount;
  ULONG FileULBytesHi,		// Only file transfer stats
				FileULBytesLo;
  ULONG FileDLBytesHi,
				FileDLBytesLo;
	ULONG	FilesUL,					// Amount of Files transfered
        FilesDL;
	};

struct FAMEInfos *FameInfo=NULL;

/*
 *  Global list structure for conference lists:
 */

struct FConf
	{
  struct 	FConf *next;
	char    ConfName[32],
					ConfLocation[102],
					ULPath[102],
					DLPath[102],
					AddULPath[102],
					AddDLPath[102];
	long		ConfNumber;
	}*fconf1,*fconf2;

/*
 *  Memorypointer for our Pool:
 */

APTR mem_pool = NULL;

/*
 *  Pointer to global FileInfoBlock structure (allocated in main() )
 */

struct FileInfoBlock *fib = NULL;

/*
 *  Preferences structure, used also by FAME-FTPdCFG:
 */

struct FTPdConfig
	{
	char	HowToUploadFile[256];		// How-To-Upload file
	ULONG	Flags1;                 // Option flags, see global_defines.h
	char  UploadTempPath[256];		// Temporary upload path
	long	FileNameLength,					// Max. allowed filename length
				FileBufferSize;					// Size for Buffer
	ULONG	Timeout;								// Timeout in seconds
	char	IPFile[256],						// Path to file containing IP Address/Hostname to use
        TitleFile[256];         // Path to ASCII/ANSI file showing login gfx
	int		PortLo,									// Passive port range, low value
				PortHi;                 // Passive port range, high value
	char	LogFile[256];						// Full filename for Logfile (not debug!)
	long  MaxUsers;								// If > 0 the Server must check for users online count
	};

struct FTPdConfig *fconfig = NULL;

const char prefspath[]="FAME:ExternEnv/Doors/FAME-FTPd.cfg";

/*
 *  UploadList contains all uploaded file that where recieved during a running session
 */

struct FTPUploads
	{
	struct	MinNode Node;
  char    FileName[128],
					FileDate[LEN_DATSTRING*2];
	long    FileSize,
					ConfNum;
	};

struct	MinList filelist;

/*
 *  Server-Status structure describes current action of FTP-Server
 */

struct FTPdStats
	{
	char	TaskAddress[10];			// FindTask(NULL)
	long	UserNumber,						// Usernumber
				CurrentConf;					// Conference Number
	char	ActionString[80];			// What the server currently doing
	long	ULFiles;             	// How many files where uploaded
	ULONG	ULBytesHi,						// Hi Long for ul bytes
				ULBytesLo;						// Lo Long for ul bytes
  char	ConnectTime[18],			// Connect time of this user
				ULName[102];					// Will be set to current upload file
	};

struct FTPdStats *fstats = NULL;	// Used by current user
struct FTPdStats *tmpptr = NULL;	// Used to find data inside datfile

/*
 *  This structure is used at the end of session, it saves the upload states
 *  as binary data for aCID-tOP so that uploads can be counted correctly.
 */

struct AcidStats
	{
	long	UserNumber,
				ConfNumber,
				ULFiles;
	ULONG	BytesHi,
				BytesLo;
	};

/*
 *  List of supported FTP commands according to RFC 959 / 2389
 */

static char *commands[] =
	{
	"user",   	/* USER: Passes the username */
	"pass",			/* PASS: Passes the password for that user */
	"type",			/* TYPE: Selects either ASCII or Binary transfer type */
	"list",     /* LIST: Displays the current directory */
	"cwd",      /*  CWD: Changes working directory */
	"quit",			/* QUIT: Disconnects user and terminates */
	"retr",			/* RETR: Send a file over network */
	"stor",			/* STOR: Retrieve a file over network */
	"port",			/* PORT: Use different data connection port */
	"nlst",			/* NLST: New list format, same as LIST for this server */
	"pwd",			/*  PWD: Print working directory */
	"xpwd",			/* XPWD: Print working directory, for compatibility with 4.2BSD */
	"mode",			/* MODE: Changes Mode, only STREAM is supported */
	"pasv",			/* PASV: Turns on Passive mode */
	"syst",			/* SYST: Returns system informations */
	"noop",			/* NOOP: Does nothing but return OK */
  "xcwd",			/* XCWD: Change working directory, for compatibility with 4.2BSD */
  "cdup",			/* CDUP: Change to upper directory */
	"size",			/* SIZE: Returns filesize of requested file (Extension) */
  "stru",			/* STRU: Structure mode for Transfers, we ony support FILE. */
	"abor",			/* ABOR: We hardly kick off all data connections..rfc959 says to do it more fancy, but who cares ;) */
	"feat",			/* FEAT: Shows extensions that are supported by this server (RFC 2389) */
  "clnt",			/* CLNT: Retrieves Client Program name and Version (Extension) */
	"help",			/* HELP: Shows list of supported commands */
	"stat",			/* STAT: Show statistics of current user */
	NULL
	};

/*
 *  Corresponding Help messages
 */

static char *cmdhelp[] =
	{
	"USER <sp> username",
	"PASS [<sp> [password]]",
	"TYPE <sp> { A | B | I | L }",
	"LIST [ <sp> path-name ]",
	"CWD [ <sp> directory-name ]",
	"QUIT (terminate service)",
	"RETR <sp> file-name",
	"STOR <sp> file-name",
	"PORT <sp> b0, b1, b2, b3, b4, b5",
	"NLST [ <sp> path-name ]",
	"PWD (return current directory)",
	"XPWD (return current directory)",
	"MODE (specify transfer mode)",
	"PASV (set server in passive mode)",
	"SYST (get type of operating system)",
	"NOOP (No Operation)",
	"XCWD [ <sp> directory-name ]",
	"CDUP (change to parent directory)",
	"SIZE <sp> path-name",
	"STRU (specify file structure)",
	"ABOR (abort operation)",
	"FEAT (List extensions supported by this Server)",
	"CLNT <sp>client_name <sp> client_version",
	"HELP [ <sp> <string> ]",
	"STAT (Show statistics of current user)",
	NULL
  };

/*
 *  FTP reply codes:
 */

char sending[]   		= "150 Opening %s mode data connection for %s\r\n";
char filesend[]			= "150 Opening %s mode data connection for '%s' (%ld bytes).\r\n";
char typeok[]    		= "200 Type %s OK\r\n";
char portok[]    		= "200 Port command okay\r\n";
char okay[]      		= "200 Ok\r\n";
char stru_ok[]			= "200 STRU F ok.\r\n";
char mode_ok[]			= "200 MODE S ok.\r\n";
char size[]					= "213 %lu\r\n";
char systreply[] 		= "215 AMIGA Type: L8\r\n";
char banner[]    		= "220- \r\n220- Welcome to %s FTP service (FAME V%ld.%ld)\r\n220- FAME-FTPd v%s [Build %ld] (c) 2003,2004 Sascha 'SieGeL/tRSi' Pfalz\r\n220- \r\n";
char bye[]       		= "221- \r\n221- \r\n\r221 Goodbye!\r\n";
char abor_ok[]			= "225 ABOR command successful.\r\n";
char rxok[]      		= "226 File received OK\r\n";
char txok[]      		= "226 Transfer complete. Sent %ld Bytes.\r\n";
char pasvcon[]			=	"227 Entering Passive Mode (%ld,%ld,%ld,%ld,%ld,%ld)\r\n";
char logged[]    		= "230 %s logged in, %s\r\n";
char cwdmsg[]    		= "250 \"%s\" is current directory\r\n";
char pwdmsg[]    		= "257 \"%s\" is current directory\r\n";
char givepass[]  		= "331 Enter Password\r\n";
char ftperror[]  		= "421 Service not available, closing control connection.\r\n";
char noconn[]    		= "425 Data connection reset\r\n";
char noopendata[] 	= "425 Can't open data connection.\r\n";
char nopasv[]				= "425 Can't open passive connection (%ld)\r\n";
char fileerror[]		= "451 Requested action aborted: local error in processing.\r\n";
char unsupp[]    		= "500 Unsupported command or option\r\n";
char badcmd[]    		= "500 Unknown command\r\n";
char only8[]     		= "501 Only logical bytesize 8 supported\r\n";
char badtype[]   		= "501 Unknown type \"%s\"\r\n";
char badport[]   		= "501 Bad port syntax\r\n";
char unimp[]     		= "502 Command not yet implemented\r\n";
char mode_fail[]		= "502 Unimplemented MODE type.\r\n";
char userfirst[] 		= "503 Login with USER first.\r\n";
char stru_fail[] 		=	"504 Unimplemented STRU type.\r\n";
char notlog[]    		= "530 Please log in with USER and PASS\r\n";
char noperm[]    		= "550 Permission denied\r\n";
char onlinewarn[]		= "550 Permission denied because you are currently online in FAME!\r\n";
char filenotfound[] = "550 Requested action not taken. File not found\r\n";
char nodir[]     		= "553 Can't read directory \"%s\"\r\n";
char cantmake[]  		= "553 Can't create \"%s\": %s\r\n";
char fileexists[]		= "553 File already exists.\r\n";
char fnametoolong[]	= "553 Filename exeeds %ld characters, skipped!\r\n";
