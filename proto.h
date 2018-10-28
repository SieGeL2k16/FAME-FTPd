/****************************************************************************************
 *  PROJECT: FAME-FTPd
 *     FILE: proto.h
 *  PURPOSE: Prototype definitions
 *  CREATED: 05-MAY-2003
 * MODIFIED: 21-JAN-2004
 *   AUTHOR: Sascha 'SieGeL' Pfalz
 ****************************************************************************************/

/*
 * 	Protos in main.c
 */

extern void ClosLibs(void);
extern char *_ProgramName;
extern void fail(long iocode, char *errstring, ...);

/*
 * 	Protos in support.c
 */

extern void 	DebugLog(char *fmt, ...);
extern STRPTR tcp_error(int error);
extern int 		readline(int fd, char *bufptr, size_t len);
extern int 		usprintf(LONG s,short stripflag, char *fmt,...);
extern void 	CutCRLF(char *s);
extern int 		usputc(LONG s,char c);
extern BOOL __regargs FormatStamp(struct DateStamp *Stamp,STRPTR DateBuffer,struct Locale *loc,BOOL listdate);
extern void 	ConvertSpaces(char *s);
extern long		GetFileSize(char *fpath);
extern long 	GetFileSizeFH(BPTR fh);
extern long 	DNSLookUp(struct sockaddr_in *, STRPTR name);
extern void   WeektopStats(void);

/*
 *  Protos in famesupport.c
 */

extern long 	RetrieveFAMEInfos(void);
extern BOOL 	ValidateFAMEUser(char *user,char *pass,struct FTP *ftp);
extern void 	FreeConferenceList(void);
extern BOOL 	AddFileToUList(char *fullname, long fsize, long cnum,struct FTP *ftp, char *tempname);
extern BOOL 	CheckForDoubles(char *fullname, long confnum);
extern long 	GetConfNumber(char *confname);
extern BOOL 	CheckIfOnline(long unumcheck);

/*
 *  Protos in MakeArray.c
 */

extern char **MakeArray (char *s,char sep);
extern void FreeArray (char **strings);
