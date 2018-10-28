/****************************************************************************************
 *  PROJECT: FAME-FTPd
 *     FILE: famesupport.c
 *  PURPOSE: FAME-related functions like username/pw validation & File Management
 *  CREATED: 05-MAY-2003
 * MODIFIED: 21-JAN-2004
 *   AUTHOR: Sascha 'SieGeL' Pfalz
 ****************************************************************************************/

#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/locale.h>
#include <proto/utility.h>
#include <proto/fame.h>
#include <libraries/fame.h>
#include <fame/fame.h>

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
#include <proto/socket.h>
#include <netinet/in.h>
#include <amitcp/socketbasetags.h>
#include <netdb.h>

#include "version.h"
#include "proto.h"
#include "ftp.h"
#include "struct_ex.h"

#include <stdlib.h>
#include <string.h>

static  char uname[]="FAME-FTPd",udesc[]="FAME FTP Server";

/*************************************************************************************************
 * Prototypes
 *************************************************************************************************/

BOOL 				ValidateFameUser(char *username, char *password, struct FTP *ftp);
long 				RetrieveFAMEInfos(void);
static long ParseConferences(char *confaxs);
BOOL 				AddConference(struct FAMEConf *, long confnumber);
void 				FreeConferenceList(void);
BOOL				AddFileToUList(char *fullname, long fsize, long cnum,struct FTP *ftp,char *tempname);
BOOL 				CheckForDoubles(char *fullname, long confnum);
long 				GetConfNumber(char *confname);
BOOL 				UpdateUserStats(long confnumber, struct FTP *ftp, ULONG bytes, long files);
BOOL 				CheckIfOnline(long unumcheck);

/*************************************************************************************************
 *     FUNCTION: RetrieveFAMEInfos()
 *      PURPOSE: Reads FAME infos like BBS Path, Sysop etc.
 * REQUIREMENTS: FAME must be started!
 *        INPUT: -
 *       RETURN: 0 => OK | -1 => FAME not running
 *        NOTES: Must be called BEFORE ValidateFAMEUser(), else system crashes!
 *************************************************************************************************/

long RetrieveFAMEInfos(void)
	{
	struct  FAMESemaphore *MyFAMESemaphore;

	FAMEMemSet(FameInfo,0,sizeof(struct FAMEInfos));
 	Forbid();
	if(MyFAMESemaphore=(struct FAMESemaphore *)FindSemaphore("FAMESemaphore"))
		{
		ObtainSemaphore((struct SignalSemaphore *)MyFAMESemaphore);
    FAMEStrCopy(MyFAMESemaphore->fsem_SVBBSName,FameInfo->BBSName,255);
		FameInfo->FAMEVersion = MyFAMESemaphore->fsem_SVVersion;
		FameInfo->FAMERevision = MyFAMESemaphore->fsem_SVRevision;
		Permit();
		ReleaseSemaphore((struct SignalSemaphore *)MyFAMESemaphore);
		return(0);
	  }
  else
		{
		Permit();
		return(-1);		// FAME not running !
		}
	}

/*************************************************************************************************
 *     FUNCTION: ValidateFAMEUser()
 *      PURPOSE: Checks if given user/password is valid/known to FAME and if given data are
 *               correct the conference list will be loaded and stored in memory.
 * REQUIREMENTS: FAME must be started!
 *        INPUT: username => Name of user
 *               password => Password of user
 *                    ftp => Master FTP struct
 *       RETURN: TRUE = User is valid and loaded | FALSE = User unknown/wrong PW
 *************************************************************************************************/

BOOL ValidateFAMEUser(char *username, char *password,struct FTP *ftp)
	{
  struct	FAMEConfigRequest *fcr;
  struct	FAMEUser					*myuserdata;
	struct 	FAMEConfAccess 		*mycfgaxs;
	struct  FAMESystem				*mysystem;
	struct	FAMELevels				*mylevel;
	long		myresult=0,rc2;
	char		errbuf[256],axs[22];
	static	char funcname[] = "ValidateFAMEUser";
	long		templevel = -1;

	if(!*username)
		{
		DebugLog("LOGIN: No username given - login denied!");
		return(FALSE);
		}
  fcr = AllocPooled(mem_pool,sizeof(struct FAMEConfigRequest));
	if(!fcr)
		{
    fail(ERROR_NO_FREE_STORE,"%s: Unable to allocate fcr struct!",funcname);
		usprintf(ftp->control,CRLF_NOSTRIP,ftperror);
 		return(FALSE);
		}
	FAMERequestReset(fcr);
	fcr->fcr_CfgUserTask 		= FindTask(NULL);
	fcr->fcr_CfgUserName 		= uname;
	fcr->fcr_CfgUserDesc 		= udesc;
  fcr->fcr_ConfigType  		= FCDD_User;							// We want the FAME user.data
	fcr->fcr_ConfigMode  		= FAMECFG_MODE_READ; 			// Nothing will be changed here!
  fcr->fcr_SearchOffset 	= 0;											// Search for the Username in user.data
	fcr->fcr_SearchValue		= username;           		// search for given Username
	fcr->fcr_SearchType    	= FAMECFGVARTYPE_STRING;  // We want string compare
	fcr->fcr_SearchOperator	= FAMECFGOP_EQUAL;				// Username must match!
	myuserdata = FAMEObtainConfig(fcr,&myresult);
	if(myuserdata)
		{
		if(!Stricmp(password,myuserdata->Password))
			{
			myresult = FAMECFGRES_SUCCESS;
			ftp->usernumber = myuserdata->UserNumber;
      FAMEStrCopy(myuserdata->ConfAccess,axs,21);
			}
		else
			{
			myresult = FAMECFGRES_SEARCHNOTFOUND;
			DebugLog("LOGIN: Unknown User %s - Login denied!",username);
			}
    }

	/*
   *  We check now if the user is allowed to login by checking his state:
 	 */

  if(myresult==FAMECFGRES_SUCCESS)
		{
    if(myuserdata->Deleted_Or_Not)		// 0 means active, all others are forbidden
			{
			myresult = FAMECFGRES_SEARCHNOTFOUND;
			}
    else
			{
			templevel = myuserdata->Userlevel;		// Save level for further checking
			}
		}
	FAMEReleaseConfig( myuserdata, FAMECFG_MODE_READ, FindTask( NULL ));
	if(myresult == FAMECFGRES_SEARCHNOTFOUND) return(FALSE);
	if(myresult != FAMECFGRES_SUCCESS)
    {
		FAMEHandleConfigResults(errbuf,myresult,FCDD_User);
		DebugLog("%s: %s (%ld)",funcname,errbuf,myresult);
		usprintf(ftp->control,CRLF_NOSTRIP,ftperror);
 		return(FALSE);
		}

	/*
	 *	Now we load the Level define for that user and check if he/she is allowed
   *  to upload files.
 	 */

	FAMERequestReset(fcr);
	fcr->fcr_CfgUserTask = FindTask(NULL);
	fcr->fcr_CfgUserName = uname;									// Tool name
	fcr->fcr_CfgUserDesc = udesc;									// Tool description
  fcr->fcr_ConfigType  = FCDD_Levels;						// We want the level structure
	fcr->fcr_ConfigMode  = FAMECFG_MODE_READ; 		// Nothing will be changed here!
	fcr->fcr_ConfigNum	 = templevel;							// Load Level Data for current user
  mylevel = FAMEObtainConfig(fcr,&myresult);
	if(mylevel)
		{
		if(mylevel->Upload) ftp->UserFlags |= UF_UPLOAD;	// Set UploadAllow Flag
		}
  else
		{
		FAMEHandleConfigResults(errbuf,myresult,FCDD_Levels);
		DebugLog("%s: %s (%ld)",funcname,errbuf,myresult);
		usprintf(ftp->control,CRLF_NOSTRIP,ftperror);
 		return(FALSE);
		}
	FAMEReleaseConfig(mylevel, FAMECFG_MODE_READ, FindTask( NULL ));

	/*
   * 	User/Password/Level access is known and valid, now request conference
   *  list for this user.
	 * 	First we need to know how many conferences exists for this BBS:
	 */

	FAMERequestReset(fcr);
	fcr->fcr_CfgUserTask = FindTask(NULL);
	fcr->fcr_CfgUserName = uname;									// Tool name
	fcr->fcr_CfgUserDesc = udesc;									// Tool description
  fcr->fcr_ConfigType  = FCDD_System;						// We want the system structure
	fcr->fcr_ConfigMode  = FAMECFG_MODE_READ; 		// Nothing will be changed here!
	mysystem = FAMEObtainConfig(fcr,&myresult);
	if(mysystem)
		{
		FameInfo->NodeCount = mysystem->NumberOfConfs;
		FAMEStrCopy(mysystem->SysOpName,FameInfo->SysOp,31);
		}
  else
		{
		FAMEHandleConfigResults(errbuf,myresult,FCDD_System);
		DebugLog("%s: %s (%ld)",funcname,errbuf,myresult);
		usprintf(ftp->control,CRLF_NOSTRIP,ftperror);
 		return(FALSE);
		}
	FAMEReleaseConfig(mysystem, FAMECFG_MODE_READ, FindTask( NULL ));

	/*
   * 	Number of conferences is known. Build up the access list for this user:
   */

	FAMERequestReset(fcr);
	fcr->fcr_CfgUserTask 	= FindTask(NULL);
	fcr->fcr_ConfigStr		= axs;                    // The conference access from FAMEUser->ConfAccess
	fcr->fcr_CfgUserName 	= uname;									// Tool name
	fcr->fcr_CfgUserDesc 	= udesc;									// Tool description
  fcr->fcr_ConfigType  	= FCDD_ConfAccess;				// We want the conference access array
	fcr->fcr_ConfigMode  	= FAMECFG_MODE_READ; 			// Nothing will be changed here!
	mycfgaxs = FAMEObtainConfig(fcr,&myresult);
	if(mycfgaxs)
  	{
		rc2 = ParseConferences(mycfgaxs->ConfFlags);
		if(rc2)
			{
			FAMEHandleConfigResults(errbuf,rc2,FCDD_Conf);
			DebugLog("%s: %s (%ld)",funcname,errbuf,rc2);
			FAMEReleaseConfig(mycfgaxs,FAMECFG_MODE_READ,FindTask(NULL));
			usprintf(ftp->control,CRLF_NOSTRIP,ftperror);
			return(FALSE);
  		}
		}
  else
		{
		FAMEHandleConfigResults(errbuf,myresult,FCDD_UserConf);
		DebugLog("%s: %s (%ld)",funcname,errbuf,myresult);
		usprintf(ftp->control,CRLF_NOSTRIP,ftperror);
 		return(FALSE);
		}
	FAMEReleaseConfig(mycfgaxs, FAMECFG_MODE_READ, FindTask( NULL ));
	FreePooled(mem_pool,fcr,sizeof(struct FAMEConfigRequest));
	return(TRUE);
	}

/*************************************************************************************************
 *     FUNCTION: ParseConferences()
 *      PURPOSE: Reads FAME conference data
 * REQUIREMENTS: Global list to hold the conference informations
 *    CALLED BY: ValidateFAMEUser()
 *        INPUT: XXX_X_XXX_ (ConfAccess array)
 *       RETURN: 0 => OK else Returncode indicates failure
 *************************************************************************************************/

static long ParseConferences(char *confaxs)
	{
  struct	FAMEConfigRequest *fcr2;
	struct	FAMEConf	*fconf;
	char 		*b = confaxs;
	long  	confcnt = 0,res2 = 0;

  fcr2 = AllocPooled(mem_pool,sizeof(struct FAMEConfigRequest));
	if(!fcr2) return(-1);
	while(*b && confcnt < FameInfo->NodeCount)
		{
		if(*b == 'X')
			{
			FAMERequestReset(fcr2);
			fcr2->fcr_CfgUserTask = FindTask(NULL);
			fcr2->fcr_CfgUserName = uname;									// Tool name
			fcr2->fcr_CfgUserDesc = udesc;									// Tool description
      fcr2->fcr_ConfigNum   = confcnt+1;							// Read data from this conference (cnfcount starts with 0!)
 	 		fcr2->fcr_ConfigType  = FCDD_Conf;							// We want the conference data
			fcr2->fcr_ConfigMode  = FAMECFG_MODE_READ; 			// Nothing will be changed here!
			fconf = FAMEObtainConfig(fcr2,&res2);
      if(fconf)
				{
				AddConference(fconf,confcnt+1);
				}
			else
				{
				FreePooled(mem_pool,fcr2,sizeof(struct FAMEConfigRequest));
				return(res2);
        }
			FAMEReleaseConfig(fconf,FAMECFG_MODE_READ,FindTask(NULL));
			}
		*b++;
		confcnt++;
		}
	FreePooled(mem_pool,fcr2,sizeof(struct FAMEConfigRequest));
  return(0);
	}

/*************************************************************************************************
 *     FUNCTION: AddConference()
 *      PURPOSE: Adds new conference to global list FConf
 * REQUIREMENTS: Global list FConf
 *        INPUT: fc					=> Pointer to FAMEConf to add
 *               confnumber	=> Corrected Conference number
 *       RETURN: TRUE = Entry added | FALSE = Out of memory
 *************************************************************************************************/

BOOL AddConference(struct FAMEConf *fc, long confnumber)
	{
	if(!fconf1)
		{
		if(!(fconf1=(struct FConf *)AllocPooled(mem_pool,sizeof(struct FConf)))) return(FALSE);
		fconf2=fconf1;
		}
	else
		{
		if(!(fconf2->next=(struct FConf *)AllocPooled(mem_pool,sizeof(struct FConf)))) return(FALSE);
		fconf2=fconf2->next;
		}
	fconf2->next=NULL;
	FAMEStrCopy(fc->ConfName,fconf2->ConfName,31);
	FAMEStrCopy(fc->ConfLocation,fconf2->ConfLocation,101);
	FAMEStrCopy(fc->UploadPath,fconf2->ULPath,101);
	FAMEStrCopy(fc->DownloadPath,fconf2->DLPath,101);
	FAMEStrCopy(fc->AddUlPaths,fconf2->AddULPath,101);
	FAMEStrCopy(fc->AddDlPaths,fconf2->AddDLPath,101);
  fconf2->ConfNumber = confnumber;
	ConvertSpaces(fconf2->ConfName);
	return(TRUE);
	}

/*************************************************************************************************
 *     FUNCTION: FreeConferenceList()
 *      PURPOSE: Adds new conference to global list FConf
 * REQUIREMENTS: Global list FConf
 *        INPUT: -
 *       RETURN: -
 *************************************************************************************************/

void FreeConferenceList(void)
	{
	struct FConf *h;

	while(fconf1)
		{
		h=fconf1;
		fconf1=fconf1->next;
		FreePooled(mem_pool,h,sizeof(struct FConf));
		}
	fconf1=NULL;fconf2=NULL;
	}

/*************************************************************************************************
 *     FUNCTION: CheckForDoubles()
 *      PURPOSE: Iterates through the filelist and searches for a given file already uploaded
 *               Also checks the dirs for the given fame conference if a file with that name
 *               already exists.
 * REQUIREMENTS: FileUploads List must be initialized!
 *        INPUT: fullname	=> Full name of uploaded file
 *               confnum  => Number of conference where we should check
 *       RETURN: TRUE = File already exists. FALSE = File cannot be found in list
 *************************************************************************************************/

BOOL CheckForDoubles(char *fullname,long confnum)
	{
  struct 	FTPUploads 	*f1;
	struct	FConf       *cdata=fconf1;
	BOOL    found=FALSE;

  for(f1=(struct FTPUploads *)filelist.mlh_Head;f1!=(struct FTPUploads *)&filelist.mlh_Tail;f1=(struct FTPUploads *)f1->Node.mln_Succ)
 		{
		if(!Stricmp(f1->FileName,FilePart(fullname)) && f1->ConfNum == confnum)
			{
			found=TRUE;
      break;
			}
  	}
	if(found==TRUE) return(found);

	// File was not found in session specific upload list, let's check now the other places:

  while(cdata)
		{
		if(confnum == cdata->ConfNumber) break;
    cdata=cdata->next;
		}
  if(!cdata)
		{
		DebugLog("Conf ptr. is ZERO???");
		return(TRUE);		// should not happen!
		}
	DebugLog("Location=%s\n1.ULPATH=%s\n1.DLPATH=%s\nn.ULPATH=%s\nn.DLPATH=%s",cdata->ConfLocation,cdata->ULPath,cdata->DLPath,cdata->AddULPath,cdata->AddDLPath);
	return(found);
	}

/*************************************************************************************************
 *     FUNCTION: AddFileToUList()
 *      PURPOSE: Adds new file to global upload list (using Exec() Lists) and copies the file
 *               to the conference playpen directory.
 * REQUIREMENTS: FileUploads List must be initialized, and file must be uploaded without errors!
 *        INPUT: fullname	=> Full name to uploaded file
 *               fsize		=> Size of File
 *               cnum			=> Conference Number
 *       RETURN: -
 *************************************************************************************************/

BOOL AddFileToUList(char *fullname, long fsize, long cnum, struct FTP *ftp, char *tempname)
	{
  struct 	FTPUploads *myul;
	struct  FConf	*fc = fconf1;
	char		buf[256],playpen[256],partul[256],d[10],t[10];
	BPTR		fp;
	BOOL		retcode;

	if(!(myul = AllocPooled(mem_pool,sizeof(struct FTPUploads))))
		{
		return(FALSE);
		}
  FAMEStrCopy(FilePart(fullname),myul->FileName,127);
	myul->FileSize = fsize;
	myul->ConfNum  = cnum;
	FormatStamp(NULL,myul->FileDate,myloc,TRUE);
	AddTail((struct List *)&filelist, (struct Node *)myul);
	while(fc)
		{
		if(fc->ConfNumber == cnum)
			{
			break;
			}
		fc=fc->next;
		}
	if(!fc)
		{
		RemTail((struct List *)&filelist);
		return(FALSE);
		}
  FAMEStrCopy(fc->ConfLocation,playpen,255);
  SPrintf(buf,"PlayPen/%s",myul->FileName);
  AddPart(playpen,buf,255);
	FAMEFillMem(buf,0,255);
	FAMEStrCopy(fc->ConfLocation,partul,255);
 	SPrintf(buf,"LostCarrier/%s@%ld",myul->FileName,ftp->usernumber);
	AddPart(partul,buf,255);
	if(!(fp = Open(partul,MODE_NEWFILE)))
		{
		DebugLog("ERROR: Cannot open %s (IoErr()=%ld",playpen,IoErr());
		RemTail((struct List *)&filelist);
    return(FALSE);
		}
	FormatStamp(NULL,buf,NULL,FALSE);
  FAMEStrMid(buf,d,1,8);
	FAMEStrMid(buf,t,9,-1);
	FPrintf(fp,"%s\n%lu\n%lu\n100\n%s\n%ld\n%s\n%s\n",myul->FileName,fsize,fsize,ftp->username,ftp->usernumber,d,t);
	Close(fp);
	retcode = FAMEDosMove(tempname,playpen,fconfig->FileBufferSize*1024,FDMF_KEEPDATA|FDMF_NODELETE);
	if(retcode == FALSE) 	/* Error while copying, so cleanup uploaded files */
		{
		DebugLog("AddUL(): DosMove() %s -> %s failed (%ld)",tempname,playpen,IoErr());
		DeleteFile(tempname);
		DeleteFile(playpen);
		DeleteFile(partul);
    }
	else
		{
		UpdateUserStats(cnum,ftp,fsize,1);
		}
  return(retcode);
	}

/*************************************************************************************************
 *     FUNCTION: GetConfNumber()
 *      PURPOSE: Returns number of conference of given conf name
 *        INPUT: confname	=> Name of conference
 *       RETURN: Conf Number or 0 to indicate "conference not found"
 *************************************************************************************************/

long GetConfNumber(char *confname)
	{
	struct	FConf *fc = fconf1;

	while(fc)
		{
    if(!Strnicmp(fc->ConfName,confname,strlen(fc->ConfName))) return(fc->ConfNumber);
		fc = fc->next;
		}
	return(0);
	}

/*************************************************************************************************
 *     FUNCTION: UpdateUserStats()
 *      PURPOSE: Updates FAME user.data with new uploaded bytes.
 *        INPUT: confnumber	=> Conference number
 *               ftp				=> Master FTP Structure
 *               bytes			=> Bytes to add
 *               files			=> Filecount to add
 *       RETURN: TRUE = Success, FALSE = Failure while updating
 *************************************************************************************************/

BOOL UpdateUserStats(long confnumber, struct FTP *ftp, ULONG bytes, long files)
	{
  struct	FAMEConfigRequest *fcr;
  struct	FAMEUser					*myuserdata;
	struct	FAMEUserConf			*myuserconf;
	long		myresult=0;
	char		errbuf[256];
	static	char funcname[] = "UpdateUserStats";
	ULONG		newbytes[2];

  fcr = AllocPooled(mem_pool,sizeof(struct FAMEConfigRequest));
	if(!fcr)
		{
    fail(ERROR_NO_FREE_STORE,"%s: Unable to allocate fcr struct!",funcname);
		usprintf(ftp->control,CRLF_NOSTRIP,ftperror);
 		return(FALSE);
		}
	FAMERequestReset(fcr);
	fcr->fcr_CfgUserTask 		= FindTask(NULL);
	fcr->fcr_CfgUserName 		= uname;
	fcr->fcr_CfgUserDesc 		= udesc;
  fcr->fcr_ConfigType  		= FCDD_User;							// We want the FAME user.data
	fcr->fcr_ConfigMode  		= FAMECFG_MODE_MODIFY;		// We have to update the datA
	fcr->fcr_ConfigNum			= ftp->usernumber;				// Usernumber to load
	myuserdata = FAMEObtainConfig(fcr,&myresult);
	if(myresult != FAMECFGRES_SUCCESS)
    {
		FAMEHandleConfigResults(errbuf,myresult,FCDD_User);
		DebugLog("%s: %s (%ld)",funcname,errbuf,myresult);
		usprintf(ftp->control,CRLF_NOSTRIP,ftperror);
		FAMEReleaseConfig( myuserdata, FAMECFG_MODE_MODIFY, FindTask( NULL ));
		FreePooled(mem_pool,fcr,sizeof(struct FAMEConfigRequest));
 		return(FALSE);
		}
  newbytes[0] = myuserdata->BytesUpHi;
	newbytes[1] = myuserdata->BytesUpload;
  FAMEAdd64(0UL,bytes,&newbytes);
	myresult = FAMELockConfig(myuserdata);
	if(myresult == FAMECFGRES_SUCCESS)
		{
		myuserdata->Uploads+=files;
		myuserdata->BytesUpHi = newbytes[0];
		myuserdata->BytesUpload = newbytes[1];
		FAMEUnLockConfig(myuserdata);
		}
	else DebugLog("LockConfig(user) Failed");
	myresult = FAMESaveConfig(myuserdata,FAMECFG_MODE_MODIFY);
	if(myresult != FAMECFGRES_SUCCESS)
		{
		DebugLog("Error writing back user.data: %ld",myresult);
		}
	FAMEReleaseConfig( myuserdata, FAMECFG_MODE_MODIFY, FindTask( NULL ));

	/* Now update UserConf.data, too */

	FAMERequestReset(fcr);
	fcr->fcr_CfgUserTask 		= FindTask(NULL);
	fcr->fcr_CfgUserName 		= uname;
	fcr->fcr_CfgUserDesc 		= udesc;
  fcr->fcr_ConfigType  		= FCDD_UserConf;					// We want the FAME user.data
	fcr->fcr_ConfigMode  		= FAMECFG_MODE_MODIFY;		// We have to update the datA
	fcr->fcr_ConfigNum			= ftp->usernumber;				// Usernumber
	fcr->fcr_ConfigNum2			= confnumber;							// Conference number
	myuserconf = FAMEObtainConfig(fcr,&myresult);
	if(myresult != FAMECFGRES_SUCCESS)
    {
		FAMEHandleConfigResults(errbuf,myresult,FCDD_UserConf);
		DebugLog("%s: %s (%ld)",funcname,errbuf,myresult);
		usprintf(ftp->control,CRLF_NOSTRIP,ftperror);
		FAMEReleaseConfig( myuserconf, FAMECFG_MODE_MODIFY, FindTask( NULL ));
		FreePooled(mem_pool,fcr,sizeof(struct FAMEConfigRequest));
 		return(FALSE);
		}
  newbytes[0] = myuserconf->BytesUpHi;
	newbytes[1] = myuserconf->BytesUpload;
  FAMEAdd64(0UL,bytes,&newbytes);
	myresult = FAMELockConfig(myuserconf);
	if(myresult == FAMECFGRES_SUCCESS)
		{
		myuserconf->Uploads+=files;
		myuserconf->BytesUpHi = newbytes[0];
		myuserconf->BytesUpload = newbytes[1];
		FAMEUnLockConfig(myuserconf);
		}
	else DebugLog("LockConfig(conf) Failed");
	myresult = FAMESaveConfig(myuserconf,FAMECFG_MODE_MODIFY);
	if(myresult != FAMECFGRES_SUCCESS)
		{
		DebugLog("Error writing back userconf.data: %ld",myresult);
		}
	FAMEReleaseConfig( myuserconf, FAMECFG_MODE_MODIFY, FindTask( NULL ));
	FreePooled(mem_pool,fcr,sizeof(struct FAMEConfigRequest));
	return(TRUE);
	}

/*************************************************************************************************
 *     FUNCTION: CheckIfOnline()
 *      PURPOSE: Tests if given User is currently online
 *        INPUT: unumcheck	=> Usernumber to check for
 *       RETURN: TRUE = User is online, FALSE = User is not online
 *************************************************************************************************/

BOOL CheckIfOnline(long unumcheck)
	{
	struct  FAMESemaphore *MyFAMESemaphore;
	struct 	FAMEInfoList 	*MyFAMEInfoList;
	BOOL		isonline=FALSE;
	long		usernumber;

	Forbid();
	if(MyFAMESemaphore=(struct FAMESemaphore *)FindSemaphore("FAMESemaphore"))
		{
		ObtainSemaphore((struct SignalSemaphore *)MyFAMESemaphore);
		Permit();
		if(MyFAMESemaphore->fsem_FirstNode)
			{
      MyFAMEInfoList=MyFAMESemaphore->fsem_FirstNode;
			do
				{
      	if((MyFAMEInfoList->fili_SVNodeOnline) && (MyFAMEInfoList->fili_UserOnline==2))
					{
					Forbid();
					usernumber = MyFAMEInfoList->fili_NodeRUser->UserNumber;
					Permit();
					if(usernumber == unumcheck)
						{
						isonline=TRUE;
						break;
						}
	        }
				MyFAMEInfoList=MyFAMEInfoList->fili_Next;
				}while(MyFAMEInfoList);
			}
		ReleaseSemaphore((struct SignalSemaphore *)MyFAMESemaphore);
		}
	else
		{
		Permit();
		}
	return(isonline);
	}
