/**************************************************************************************************
 *  PROJECT: FAME-FTPd
 *     FILE: global_defines.h
 *  PURPOSE: All defines used in FTPd and Configuration program
 *  CREATED: 05-MAY-2003
 * MODIFIED: 25-JAN-2004
 *   AUTHOR: Sascha 'SieGeL' Pfalz
 **************************************************************************************************/

#define	BLKSIZE			4096								/* Chunk size for file I/O */
#define CMDBUF_SIZE	1024  							/* Size for read buffer */
#define CMDBUF_TEMP 2048								/* Size for temporary read buffer (max MTU) */

#define	ASCII_TYPE		0									/* ASCII Type */
#define	IMAGE_TYPE		1									/* Binary Type */
#define	LOGICAL_TYPE	2									/* Logical type (Binary) */

#define IPPORT_FTPD 20									/* Default FTP Data port */
#define CTLZ		26											/* EOF for CP/M systems */

#define UC(b) (((int) b) & 0xff)        /* Define used to mask out PORT/PASV ports */

#define EOF (-1)												/* End-of-line indicator, see stdio.h */
#define	LOG_FTP			(11<<3)							/* ftp daemon (missed in Amitcp headers?) */
#define OK (0)                          /* OK define ;) */
#define SOCKETNAME "bsdsocket.library"	/* Name of BSD socket library */

#define FILENAME_LENGTH	28              /* Max. allowed length of filenames (B514+) */

#define CONNECT_RETRY	5     						/* For data connections we try it more than once / Used also for NSLookup-retry */
#define CONNECT_WAIT	20    						/* We wait 20/50s between two connection attempts */

#define CRLF_STRIP		0									/* for usprintf() -> Kill all CRLF and append our own */
#define CRLF_NOSTRIP	1     						/* for usprintf() -> Send merged string as passed */

#define SetError(x)((struct Process *)FindTask(NULL))->pr_Result2 = x	/* Define to set result before exit() */

#define STATS_FILE_RETRY		10						/* Retry Counter when accessing stats file */
#define STATS_RETRY_DELAY		25						/* Retry delay in x/50 seconds before retry accessing stats file */
#define STATS_TASKADDRESS		0             /* Field 0 of Statusfile -> Task Address */
#define	STATS_USERID				1							/* Field 1 of Statusfile -> UserNumber */
#define	STATS_ACTIONSTRING	2             /* Field 2 of Statusfile -> Action String */
#define STATS_ULFILES       3							/* Amount of Files uploaded this session */
#define STATS_ULBYTES				4             /* Amount of Bytes uploaded this session */
#define STATS_CONNECTTIME		5							/* Date string in USA format when user connected */

#define WTS_RETRY						10            /* How often we will retry to save our stats */
#define WTS_RETRY_DELAY			25            /* Dely in 1/50s between retries */

/**************************************************************************************************
 * Defines for all supported FTP Commands
 **************************************************************************************************/

#define CMD_USER 	0
#define CMD_PASS 	1
#define CMD_TYPE	2
#define CMD_LIST	3
#define CMD_CWD		4
#define	CMD_QUIT  5
#define CMD_RETR  6
#define CMD_STOR  7
#define CMD_PORT  8
#define CMD_NLST  9
#define CMD_PWD		10
#define CMD_XPWD	11
#define CMD_MODE	12
#define CMD_PASV	13
#define CMD_SYST	14
#define CMD_NOOP	15
#define CMD_XCWD  16
#define CMD_CDUP	17
#define CMD_SIZE	18
#define CMD_STRU	29
#define CMD_ABOR	20
#define CMD_FEAT	21
#define CMD_CLNT	22
#define CMD_HELP	23
#define CMD_STAT	24

/**************************************************************************************************
 * Defines for FTPdConfig->Flags1:
 **************************************************************************************************/

#define	CFG_USEDEBUG	(1 << 0)        			/* If we should open the Debug Window */
#define CFG_USEDNS		(1 << 1)          		/* If we should resolve hostnames */
#define CFG_WEEKTOP		(1 << 2)							/* Support for aCID-tOP? */

/**************************************************************************************************
 * Defines used in Configuration tool:
 **************************************************************************************************/

#define REG(x) register __ ## x
#define ASM    __asm
#define SAVEDS __saveds
#ifndef MAKE_ID
#define MAKE_ID(a,b,c,d) ((ULONG) (a)<<24 | (ULONG) (b)<<16 | (ULONG) (c)<<8 | (ULONG) (d))
#endif

#define ID_SAVE					1										/* "Save" Button */
#define ID_ABOUT 				2										/* "About" menu entry */
#define ID_ABOUT_MUI		3										/* "About MUI..." menu entry */
#define ID_ABOUTCLOSE		4                   /* "Cancel" button/CloseWindow/ESC */
#define ID_UPDATE				5										/* "Update Task Monitor" menu entry */
#define ID_DOUBLESTART 	666									/* To recognize double app start */
#define ID_ULHELPFILE		900                	/* Unique ID for "How_To_Upload.txt" popstring */
#define ID_ULTEMPDIR		901									/* Unique ID for "Upload temp path" popstring */
#define ID_IPFILE				902                 /* Unique ID for "Use IP from this File" popstring */
#define ID_TITLEFILE		903									/* Unique ID for "Login ASCII:" popstring */
#define ID_LOGDIR				904									/* Unique ID for "Logfile path:" popstring */
#define ID_STATSDIR			905									/* Unique ID for "Statistics path:" popstring */
