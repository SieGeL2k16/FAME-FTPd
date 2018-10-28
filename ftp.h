/****************************************************************************************
 *  PROJECT: FAME-FTPd
 *     FILE: ftp.h
 *  PURPOSE: FTP related structures and global variables
 *  CREATED: 05-MAY-2003
 * MODIFIED: 05-SEP-2003
 *   AUTHOR: Sascha 'SieGeL' Pfalz
 ****************************************************************************************/

/*
 *  Master FTP structure, all ftp related data will be stored in this struct
 */

struct FTP
	{
	LONG		control;
	LONG		data;
	LONG		pdata;
	char		type;
	int 		logbsize;							/* Logical byte size for logical type */
	struct 	sockaddr_in port;			/* Remote port for data connection */
	struct 	sockaddr_in pasv;			/* For Passive Mode */
	struct	sockaddr_in ctrl;			/* Our Control structure (our own) */
	char 		username[32];					/* Arg to USER command */
	char		password[32];       	/* Arg to PASS command */
	long		usernumber;						/* Usernumber from fAME */
	char 		cd[256],							/* Current working directory */
					IPaddr[22],						/* User's IP Address */
					hostname[256],				/* User's resolved Hostname, if possible */
					ClientVersion[128];		/* User's FTP client program, if passed via CLNT command */
  ULONG		UserFlags;						/* User Flags (see below for defines) */
	};

#define UF_UPLOAD	(1 << 0)


/* See struct.h for command & help tables */

