/****************************************************************************************
 *  PROJECT: FAME-FTPd
 *     FILE: struct_ex.h
 *  PURPOSE: FTP related structures and global variables - external definitions
 *  CREATED: 05-MAY-2003
 * MODIFIED: 24-APR-2004
 *   AUTHOR: Sascha 'SieGeL' Pfalz
 ****************************************************************************************/

#include "global_defines.h"

extern struct Library 			*SocketBase;
extern struct Library 			*UtilityBase;
extern struct FAMELibrary		*FAMEBase;
extern struct LocaleBase 		*LocaleBase;
extern struct Locale 				*myloc;
extern struct Library 			*SysLogBase;
extern struct IntuitionBase	*IntuitionBase; /* Required for EasyRequest() */

/*
 *  server_socket is our socket comming from InetD
 */

extern LONG 	server_socket;

/*
 *  As we initialize bsdsocket.library we have to supply also our own error vars
 */

extern int 	errno,h_errno;
extern char *_ProgramName;

/*
 *  Memory pointers for general I/O
 */

extern char *readbuf;
extern char *readtmp;
extern char *uspbuf;
extern APTR filebuf;

/*  Our global FAME info structure, will be filled by Obtaining() FAME semaphore
 *  and copy all informations from the semaphore into this structure
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

extern struct FAMEInfos *FameInfo;

/*
 *  Global list structure for conference lists:
 */

extern struct FConf
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
 *  Preferences structure, used also by FAME-FTPdCFG:
 */

extern struct FTPdConfig
	{
	char	HowToUploadFile[256];		// How-To-Upload file
	ULONG	Flags1;                 // Option flags, see global_defines.h
	char  UploadTempPath[256];		// Temporary upload path
	long	FileNameLength,					// Max. allowed filename length
				FileBufferSize;					// Size for Buffer
	ULONG	Timeout;								// Timeout in seconds
	char	IPFile[256],						// Path to file containing IP Address to use
        TitleFile[256];         // Path to ASCII/ANSI file showing login gfx
	int		PortLo,									// Passive port range, low value
				PortHi;                 // Passive port range, high value
	char	LogFile[256];						// Full filename for Logfile (not debug!)
	long  MaxUsers;								// If > 0 the Server must check for users online count
	};

extern struct FTPdConfig *fconfig;

/*
 *  Memorypointer for our Pool:
 */

extern APTR mem_pool;

/*
 *  Pointer to global FileInfoBlock structure (allocated in main() )
 */

extern struct FileInfoBlock *fib;

/*
 *  UploadList contains all uploaded file that where recieved during a running session
 */

extern struct FTPUploads
	{
	struct	MinNode Node;
  char    FileName[128],
					FileDate[LEN_DATSTRING*2];
	long    FileSize,
					ConfNum;
	};

extern struct	MinList filelist;

extern struct FTPdStats
	{
	char	TaskAddress[10];
	long	UserNumber,
				CurrentConf;					// Conference Number
	char	ActionString[80];
	long	ULFiles;
	ULONG	ULBytesHi,
				ULBytesLo;
  char	ConnectTime[18],
				ULName[102];					// Will be set to current upload file
	};

extern struct FTPdStats *fstats;	// Used by current user
extern struct FTPdStats *tmpptr;	// Used to find data inside datfile

/*
 *  This structure is used at the end of session, it saves the upload states
 *  as binary data for aCID-tOP so that uploads can be counted correctly.
 */

extern struct AcidStats
	{
	long	UserNumber,
				ConfNumber,
				ULFiles;
	ULONG	BytesHi,
				BytesLo;
	};


/* FTP Response codes */

extern char sending[];			/* 150 */
extern char filesend[];     /* 150 */
extern char typeok[];				/* 200 */
extern char portok[];				/* 200 */
extern char okay[];					/* 200 */
extern char stru_ok[];			/* 200 */
extern char mode_ok[];			/* 200 */
extern char size[];					/* 213 */
extern char systreply[];		/* 215 */
extern char banner[];				/* 220 */
extern char bye[];					/* 221 */
extern char abor_ok[];			/* 225 */
extern char rxok[];					/* 226 */
extern char txok[];					/* 226 */
extern char pasvcon[];			/* 227 */
extern char logged[];				/* 230 */
extern char cwdmsg[];				/* 250 */
extern char pwdmsg[];				/* 257 */
extern char givepass[];			/* 331 */
extern char ftperror[];			/* 421 */
extern char noconn[];				/* 425 */
extern char noopendata[];   /* 425 */
extern char nopasv[];				/* 425 */
extern char fileerror[];		/* 451 */
extern char unsupp[];				/* 500 */
extern char badcmd[];				/* 500 */
extern char only8[];				/* 501 */
extern char badtype[];			/* 501 */
extern char badport[];			/* 501 */
extern char unimp[];				/* 502 */
extern char mode_fail[];		/* 502 */
extern char userfirst[];		/* 503 */
extern char stru_fail[];		/* 504 */
extern char notlog[];				/* 530 */
extern char noperm[];				/* 550 */
extern char onlinewarn[];		/* 550 */
extern char filenotfound[];	/* 550 */
extern char nodir[];        /* 553 */
extern char cantmake[];			/* 553 */
extern char fileexists[];		/* 553 */
