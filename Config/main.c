/****************************************************************************************
 *  PROJECT: FAME-FTPd
 *     FILE: main.c
 *  PURPOSE: FAME-FTPdCFG - Configuration tool for FAME-FTPd
 *  CREATED: 03-AUG-2003
 * MODIFIED: 24-APR-2004
 *   AUTHOR: Sascha 'SieGeL' Pfalz
 ****************************************************************************************/

#include <proto/muimaster.h>
#include <mui/InfoText_mcc.h>
#include <MUI/nlistview_mcc.h>
#include <MUI/nlist_mcc.h>
#include <proto/dos.h>
#include <proto/exec.h>
#include <proto/asl.h>
#include <proto/fame.h>
#include <proto/intuition.h>
#include <libraries/fame.h>
#include <fame/fame.h>
#include <exec/memory.h>
#include <libraries/mui.h>
#include <libraries/asl.h>
#include <libraries/fame.h>
#include "version.h"
#include "/struct.h"
#include "/global_defines.h"
#include "inno.h"

struct	Library *MUIMasterBase=NULL;

LONG 		__stack = 16184L;													// Ab MUI V3.0 ABSOLUT ZWINGEND!!!

APTR 		app,mainwindow=NULL,strip,aboutwin,adv_lv,adv_txt,fnamelength,fbufsize;

APTR 		howto,debugwin,usedns,uldir,timeout,			// All configuration options
				ipfile,titlefile,portlo,porthi,
				logdir,maxusers,weektop,
				savebtn,cancelbtn;

STRPTR 	Pages[]={"Pathes","Options","Task Monitor",NULL};

static	char *docpathes[]={"PROGDIR:FAME-FTPd.guide","FAME:Documentation/FAME-FTPd.guide",NULL};

/**************************************************************************************************
 * Prototypes
 **************************************************************************************************/

void 		main(void);
void 		__autoopenfail(void) { _XCEXIT(0);}
void 		fail(APTR, char *,BOOL);
void 		SPrintf(char *Buffer,char *ctl, ...) { RawDoFmt(ctl, (long *)(&ctl + 1), (void (*))"\x16\xc0\x4e\x75",Buffer);}
void 		GetUserName(long userid, char *buffer);
void 		GetConfName(long confid, char *buffer);

STATIC 	void ShowAbout(void);								// Displays the About Window
STATIC 	void RemoveAboutWin(void);     			// Removes About Window
STATIC 	void DoubleStart(void);        			// Avoids double starts
STATIC 	void SavePrefs(void);								// Saves preferences
STATIC 	void LoadPrefs(void);								// Loads preferences
STATIC 	void UpdateStatDisplay(void);

/* Help functions for easier creation of MUI objects */

APTR 		CreatePopObject(long num,char key,char *help);
APTR 		CreateKeyButton(char *label, char key,char *help);
APTR 		CreateKeyCheckMark(BOOL status, char key, char *help);
APTR 		CreateInteger(char key,long weight,long maxlen,long contents,char *help);

/* Hooks for Filerequester and NewList */

SAVEDS 	ASM ULONG 	PrefsPopupFunc(REG(a0) struct Hook *hook, REG(a1) void *args, REG(a2) APTR obj);
SAVEDS 	ASM struct	FTPdStats *adv_confunc(REG(a0) struct Hook *hook,REG(a2) APTR mem_pool, REG(a1) struct FTPdStats *fs);
SAVEDS 	ASM long 		adv_desfunc(REG(a0) struct Hook *hook,REG(a2) APTR mem_pool,REG(a1) struct FTPdStats *line);
SAVEDS 	ASM long 		adv_dspfunc(REG(a0) struct Hook *hook,REG(a2) char **array,REG(a1) struct FTPdStats *line);

static 	struct Hook adv_conhook			=	{{NULL, NULL},(void *)adv_confunc,NULL,NULL};
static 	struct Hook adv_deshook			=	{{NULL, NULL},(void *)adv_desfunc,NULL,NULL};
static 	struct Hook adv_dsphook			=	{{NULL, NULL},(void *)adv_dspfunc,NULL,NULL};
static 	struct Hook PrefsPopupHook 	= {{NULL, NULL},(void *)PrefsPopupFunc,	NULL, NULL};

/**************************************************************************************************
 * FUNCTION: main()
 *  PURPOSE: Start of program
 *    INPUT: none
 *   RETURN: none
 **************************************************************************************************/

void main(void)
	{
	char	buffer[256],docpath[256];
	APTR	mypic,myregister;
	BPTR	testit;

	if(!(IntuitionBase=(struct IntuitionBase *) OpenLibrary("intuition.library",39L))) { exit(20); }
	if(!(MUIMasterBase=OpenLibrary(MUIMASTER_NAME,MUIMASTER_VMIN))) fail(NULL,"Failed to open "MUIMASTER_NAME" V12++ !",TRUE);
  if(!(UtilityBase=OpenLibrary("utility.library",37L))) fail(NULL,"Failed to open utility.library V37++ !",TRUE);
	if(!(FAMEBase=(struct FAMELibrary *)OpenLibrary(FAMENAME,4))) fail(NULL,"Failed to open FAME.library V4++ !!!",TRUE);
	if(!(mem_pool=CreatePool(MEMF_CLEAR|MEMF_PUBLIC,20480L,20480L))) fail(NULL,"Cannot allocate 20k memory pool??",TRUE);
	if(!(fconfig = AllocPooled(mem_pool,sizeof(struct FTPdConfig)))) fail(NULL,"Cannot allocate prefs struct!",TRUE);
	if(!(fib = AllocDosObject(DOS_FIB,NULL)))
		{
		fail(NULL,"Cannot allocate FIB struct!",TRUE);
		}
	testit = Lock(docpathes[0],ACCESS_READ);
	if(testit)
		{
		UnLock(testit);
		FAMEStrCopy(docpathes[0],docpath,255);
		}
	else FAMEStrCopy(docpathes[1],docpath,255);
	app = ApplicationObject,
		MUIA_Application_Title      , "FAME-FTPdCFG",
		MUIA_Application_Version    , "$VER: FAME-FTPdCFG "COMPILE_VERSION" ("COMPILE_DATE") ["CPU_TYPE"]\0",
		MUIA_Application_Copyright  , "Copyright ©2003-2004, Sascha 'SieGeL/tRSi' Pfalz",
		MUIA_Application_Author     , "Sascha 'SieGeL/tRSi' Pfalz",
		MUIA_Application_Description, "Configuration for FAME-FTPd",
		MUIA_Application_Base       , "FAME-FTPdCFG",
		MUIA_Application_SingleTask , TRUE,
		MUIA_Application_HelpFile		, docpath,
 		MUIA_Application_Menustrip	, strip = MenustripObject,
			MUIA_Family_Child					, MenuObjectT("Project"),
	  		MUIA_Family_Child				, MenuitemObject,
			  	MUIA_Menuitem_Title   , "Save",
				  MUIA_Menuitem_Shortcut, "S",
					MUIA_UserData					,	ID_SAVE,
			  End,
	  		MUIA_Family_Child				, MenuitemObject,
			  	MUIA_Menuitem_Title   , "Update Task Monitor",
				  MUIA_Menuitem_Shortcut, "U",
					MUIA_UserData					,	ID_UPDATE,
			  End,
		  	MUIA_Family_Child				, MenuitemObject,
					MUIA_Menuitem_Title		, NM_BARLABEL,
			  End,
		  	MUIA_Family_Child				, MenuitemObject,
			  	MUIA_Menuitem_Title   , "About",
				  MUIA_Menuitem_Shortcut, "?",
					MUIA_UserData					,	ID_ABOUT,
			  End,
		  	MUIA_Family_Child				, MenuitemObject,
			  	MUIA_Menuitem_Title   , "About MUI...",
				  MUIA_Menuitem_Shortcut, "!",
					MUIA_UserData					,	ID_ABOUT_MUI,
			  End,
		  	MUIA_Family_Child				, MenuitemObject,
					MUIA_Menuitem_Title		, NM_BARLABEL,
			  End,
		  	MUIA_Family_Child				, MenuitemObject,
			  	MUIA_Menuitem_Title   , "Quit",
				  MUIA_Menuitem_Shortcut, "Q",
					MUIA_UserData					,	MUIV_Application_ReturnID_Quit,
			  End,
	    End,
		End,
		SubWindow, mainwindow = WindowObject,
			MUIA_Window_ID         , MAKE_ID('M','A','I','N'),
			MUIA_Window_Title      , "FAME-FTPdCFG V"COMPILE_VERSION" by Sascha 'SieGeL/tRSi' Pfalz",
			MUIA_Window_AppWindow  , FALSE,
			MUIA_Window_ScreenTitle,"FAME-FTPdCFG V"COMPILE_VERSION" ("COMPILE_DATE") ["CPU_TYPE"]\0",
			WindowContents, VGroup,
				Child, mypic=TextObject,TextFrame,MUIA_Background,MUII_SHADOWFILL,End,
				Child, myregister=RegisterGroup(Pages),
					MUIA_Register_Frame, TRUE,
					Child, VGroup,
						Child, ColGroup(2),
							Child, KeyLabel2("Login ASCII:"							,'a'),Child,titlefile = CreatePopObject(ID_TITLEFILE	,'a',"CONFIG"),
							Child, KeyLabel2("Upload Description File:"	,'h'),Child,howto 		= CreatePopObject(ID_ULHELPFILE	,'h',"CONFIG"),
							Child, KeyLabel2("Temporary Upload Path:"		,'t'),Child,uldir 		= CreatePopObject(ID_ULTEMPDIR	,'t',"CONFIG"),
							Child, KeyLabel2("Logfile:"									,'l'),Child,logdir 		= CreatePopObject(ID_LOGDIR			,'l',"CONFIG"),
							Child, KeyLabel2("Use passive IP from file:",'i'),Child,ipfile		= CreatePopObject(ID_IPFILE			,'i',"CONFIG"),
						End,
					End,
					Child, VGroup,
            Child, VGroup,
							Child, ColGroup(4),GroupFrame,
								Child, KeyLabel1("Use Debug-Window:"	,'d'),Child,debugwin= CreateKeyCheckMark(FALSE	,'d',"CONFIG"),
								Child, KeyLabel1("Resolve Hostnames:"	,'r'),Child,usedns	= CreateKeyCheckMark(TRUE	,'r',"CONFIG"),
								Child, KeyLabel1("aCID-tOP sUPPORT:"	,'a'),Child,weektop	= CreateKeyCheckMark(FALSE	,'a',"CONFIG"),
								Child, Label("Max. Filename length:"),	Child,fnamelength = MUI_MakeObject(MUIO_NumericButton,"Max. Filename length:",12,107,"%3ld"),
								Child, Label("Filecopy Buffer Size:"),	Child,fbufsize		= MUI_MakeObject(MUIO_NumericButton,"Max. Filename length:",256,1024,"%4ldkb"),
								Child, Label("Timeout in Minutes:"),		Child,timeout			= MUI_MakeObject(MUIO_NumericButton,"Timeout in Minutes:",1,15,"%ldmin"),
							End,
							Child, ColGroup(4), GroupFrameT("Passive port range"),
								Child, KeyLabel1("From:",'f'),
								Child, portlo 	= CreateInteger('f',100,5,0,"CONFIG"),
                Child, KeyLabel1("To:",'t'),
								Child, porthi 	= CreateInteger('t',100,5,0,"CONFIG"),
							End,
							Child, ColGroup(2), GroupFrame,
								Child, KeyLabel1("Max. User allowed:",'M'),
								Child, maxusers = CreateInteger('M',100,5,0,"CONFIG"),
              End,
  					End,
					End,
					Child, VGroup, GroupFrameT("Task Monitor"),
						Child, adv_txt= TextObject, TextFrame, MUIA_Background, MUII_TextBack,End,
						Child, adv_lv = NListviewObject,
  	        	MUIA_ShortHelp,							"Shows active connections on your FAME-FTPd Server",
							MUIA_NListview_NList,				NListObject,
    					MUIA_NList_Input,						TRUE,
							MUIA_NList_DoubleClick,			TRUE,
							MUIA_HelpNode,							"ADV_LV",
							InputListFrame,
								MUIA_List_ConstructHook,	&adv_conhook,
								MUIA_List_DestructHook,		&adv_deshook,
								MUIA_List_DisplayHook,		&adv_dsphook,
								MUIA_List_Format,					"P=\33l BAR,P=\33l BAR,P=\33l BAR,P=\33l BAR,P=\33l",
								MUIA_List_Title,					TRUE,
								ReadListFrame,
							End,
						End,
					End,
				End,
				Child, VGroup, GroupFrame,
					Child, ColGroup(2),
		 				MUIA_Group_SameSize, TRUE,
						Child, savebtn	= CreateKeyButton("Save Prefs",'s',"SAVE_PREFS"),
						Child, cancelbtn= CreateKeyButton("Cancel",'c',"CANCEL_PREFS"),
					End,
				End,
			End,
		End,
	End;
	if(!app) fail(app,"Unable to create Main window!",TRUE);
	LoadPrefs();
 	DoMethod(mainwindow	,MUIM_Notify,MUIA_Window_CloseRequest,TRUE,	app,2,MUIM_Application_ReturnID,MUIV_Application_ReturnID_Quit);
  DoMethod(cancelbtn 	,MUIM_Notify,MUIA_Pressed,FALSE,           	app,2,MUIM_Application_ReturnID,MUIV_Application_ReturnID_Quit);
	DoMethod(savebtn   	,MUIM_Notify,MUIA_Pressed,FALSE,         		app,2,MUIM_Application_ReturnID,ID_SAVE);
	DoMethod(app				,MUIM_Notify,MUIA_Application_DoubleStart, MUIV_EveryTime,app,2,MUIM_Application_ReturnID,ID_DOUBLESTART);
	DoMethod(mypic			,MUIM_SetAsString,MUIA_Text_Contents,"\33c\0338\33bFAME-FTPdCFG V%s Build %ld (%s)\33n\nwritten by Sascha 'SieGeL/tRSi' Pfalz",COMPILE_VERSION,COMPILE_BUILD,COMPILE_DATE);

	// Install Bubble Help texts:

	set(howto,				MUIA_ShortHelp,		"This file will be displayed\nin every conference as a short help.\nGive here the full path to your textfile.\nDefaults to HOW_TO_UPLOAD.TXT");
	set(uldir,				MUIA_ShortHelp,		"Enter here the path where all temporary uploads should be placed.\nA good choice is RAM: to avoid disk fragmentation.");
	set(debugwin,			MUIA_ShortHelp,		"Enable this option to automatically open a console\nwindow on your WB screen for debugging purposes.");
	set(usedns,				MUIA_ShortHelp,		"Enable this option to have all connected IP addresses\nresolved to their Hostnames (if possible).\nThis may take some time if the DNS is slow, so you may disable it.\nAffects all logs, too!");
	set(fnamelength,	MUIA_ShortHelp,		"You can define here the maximum allowed filename\nlength that can be used for uploaded files.\nPlease do not use filenames > 31 characters,\nas most doors cannot handle this correctly!");
	set(fbufsize,			MUIA_ShortHelp,		"Choose the buffer size for copying uploaded files to the FAME directories.\nYou can choose between 256kb - 1024kb.");
	set(timeout,			MUIA_ShortHelp,		"Choose how many minutes the server should wait until a timeout occures.\nRecommended setting is 10 minutes.");
	set(cancelbtn,		MUIA_ShortHelp,		"Cancels Preferences and exits.");
	set(ipfile,				MUIA_ShortHelp,		"Enter here the full path and name to a file\ncontaining an ip address which should be used\nfor passive responses. Leave empty to use the\nip address of the interface the connection was\nestablished.\nYou can of course add also a hostname to that file\n(i.e. blub.dyndns.org) the FTP Server will try to\nresolve the hostname before using it.");
	set(titlefile,		MUIA_ShortHelp,		"You may optionally enter here\nan ASCII / ANSI file which will\nbe shown to the client right\nafter connecting.");
	set(portlo,				MUIA_ShortHelp,		"You may specify a port range to use for passive connections.\nA value of 0 means that the TCP stack will auto-choose the port numbers.\nIf you are however behind a firewall you may need to explictly specify what\nports can be used. Please specify in this case range between 1-32767 for \nport usage. Of course the minimum portrange must be lower than maxmimum\nand you should also make sure to have at least 5 ports free, else you may\nrun into trouble! Normal users should leave both values at default value of 0.");
	set(porthi,				MUIA_ShortHelp,		"You may specify a port range to use for passive connections.\nA value of 0 means that the TCP stack will auto-choose the port numbers.\nIf you are however behind a firewall you may need to explictly specify what\nports can be used. Please specify in this case range between 1-32767 for \nport usage. Of course the minimum portrange must be lower than maxmimum\nand you should also make sure to have at least 5 ports free, else you may\nrun into trouble! Normal users should leave both values at default value of 0.");
	set(weektop,			MUIA_ShortHelp,		"If you have aCID-tOP installed enable this option.\nThe FTP server will write special files after every upload\nso that the aCID-tOP tool can count also your FTP uploads.\nThis of course works only with my aCID-tOP tool.");
  set(logdir,				MUIA_ShortHelp,		"Enter here a filename where the FTPServer should write the logfile.");
	set(maxusers,			MUIA_ShortHelp,		"You can enter here a maximum count of simultaenous online users.\nIf this limit is reached no new connections are allowed.\nSet to 0 to disable MaxUser check");

	SPrintf(buffer,"Saves the preferences file to\n%s",prefspath);
	set(savebtn,  		MUIA_ShortHelp, 	buffer);

	// Finally set all preferences data to our GUI objects:

  set(howto			,	MUIA_String_Contents,fconfig->HowToUploadFile);
	set(uldir			, MUIA_String_Contents,fconfig->UploadTempPath);
	set(ipfile		, MUIA_String_Contents,fconfig->IPFile);
	set(titlefile	, MUIA_String_Contents,fconfig->TitleFile);
	set(logdir		, MUIA_String_Contents,fconfig->LogFile);

	if(fconfig->Flags1 & CFG_USEDEBUG) setcheckmark(debugwin,TRUE);
	else setcheckmark(debugwin,FALSE);
	if(fconfig->Flags1 & CFG_USEDNS)	 setcheckmark(usedns,TRUE);
	else setcheckmark(usedns,FALSE);
	if(fconfig->Flags1 & CFG_WEEKTOP)	setcheckmark(weektop,TRUE);
	else setcheckmark(weektop,FALSE);
	set(fnamelength	,MUIA_Numeric_Value,fconfig->FileNameLength);
	set(fbufsize		,MUIA_Numeric_Value,fconfig->FileBufferSize);
	set(timeout			,MUIA_Numeric_Value,fconfig->Timeout/60);

  SPrintf(buffer,"%ld",fconfig->PortLo); 		set(portlo	,MUIA_String_Contents,buffer);
	SPrintf(buffer,"%ld",fconfig->PortHi); 		set(porthi	,MUIA_String_Contents,buffer);
  SPrintf(buffer,"%ld",fconfig->MaxUsers);	set(maxusers,MUIA_String_Contents,buffer);

	// And now: open the main window :)

	set(mainwindow,MUIA_Window_Open,TRUE);
	UpdateStatDisplay();
	{
	ULONG sigs = 0;
  ULONG	retcode;

	do
		{
		retcode = DoMethod(app,MUIM_Application_NewInput,&sigs);
		if (sigs)
			{
			sigs = Wait(sigs | SIGBREAKF_CTRL_C);
			if (sigs & SIGBREAKF_CTRL_C) break;
			}
		switch(retcode)
			{
			case ID_SAVE:					SavePrefs();
														break;

			case ID_ABOUT:				ShowAbout();
														break;

			case ID_ABOUTCLOSE:		RemoveAboutWin();
														break;

			case ID_ABOUT_MUI:		if(!aboutwin)
  	                      		{
															aboutwin=AboutmuiObject,
      	    	               		MUIA_Window_RefWindow,mainwindow,
																MUIA_Aboutmui_Application,app,
          	                 	End;
															}
														if(aboutwin) set(aboutwin,MUIA_Window_Open,TRUE);
														else fail(app,"Unable to open MUI About window",TRUE);
														break;

			case ID_DOUBLESTART:  DoubleStart();
                            break;

			case ID_UPDATE:				UpdateStatDisplay();
														break;
			}
		}while(retcode != MUIV_Application_ReturnID_Quit);
	}
 	set(mainwindow,MUIA_Window_Open,FALSE);
	fail(app,"",TRUE);
	}

/**************************************************************************************************
 * FUNCTION: PrefsPopUpFunc()
 *  PURPOSE: Hook function for File-PopUp objects
 *    INPUT: hook		=> Pointer to hook structure PrefsPopupHook
 *           args		=> Arguments passed (not used)
 *           obj		=> Pointer to MUI application
 *   RETURN: ULONG	=> Always return 0, error conditions are handled here
 **************************************************************************************************/

SAVEDS ASM ULONG PrefsPopupFunc(REG(a0) struct Hook *hook, REG(a1) void *args, REG(a2) APTR obj)
	{
  struct 	Window *window;
  struct 	FileRequester *req;
	char		*mybuf,FileBuffer[256],buf[512];
	long		myid;
	BOOL    onlydirs = FALSE;

	*buf=*FileBuffer='\0';
  set(app,MUIA_Application_Sleep,TRUE);
  get(mainwindow,MUIA_Window  ,&window);
	get(obj,MUIA_String_Contents,&mybuf);
	get(obj,MUIA_UserData,&myid);
  switch(myid)
		{
		case	ID_TITLEFILE:
		case	ID_IPFILE:
		case	ID_ULHELPFILE:  
		case	ID_LOGDIR:			onlydirs = FALSE;
													FAMEStrCopy(FilePart(mybuf),FileBuffer,255);
                          break;
		case	ID_ULTEMPDIR:		onlydirs = TRUE;
                          break;
		}
	if(*FileBuffer)
		{
		FAMEStrMid(mybuf,buf,1,strlen(mybuf)-strlen(FileBuffer));
		}
	else
		{
		FAMEStrCopy(mybuf,buf,255);
		}
  if(req=MUI_AllocAslRequestTags(ASL_FileRequest,TAG_DONE))
		{
    if (MUI_AslRequestTags(req,	ASLFO_Window        ,window,
         												ASLFO_PrivateIDCMP  ,TRUE,
												        ASLFO_TitleText     ,"Select file...",
																ASLFR_InitialDrawer	,buf,
																ASLFR_InitialFile		,FileBuffer,
																ASLFR_DoPatterns		,TRUE,
																ASLFR_RejectIcons		,TRUE,
																ASLFR_DrawersOnly		,onlydirs,
																TAG_DONE))
					{
					FAMEFillMem(buf,0,512);
					FAMEStrCopy(req->fr_Drawer,buf,255);
					if(onlydirs==FALSE) AddPart(buf,req->fr_File, 512);
					set(obj,MUIA_String_Contents,buf);
    			}
    MUI_FreeAslRequest(req);
  	}
  set(app,MUIA_Application_Sleep,FALSE);
  return(0);
	}

/**************************************************************************************************
 * FUNCTION: adv_confunc()
 *  PURPOSE: Hook function for NewList objects - Constructor
 *    INPUT: hook			=> Pointer to hook structure
 *           mem_pool	=> Global memory pool pointer
 *           fs				=> Structure to add to display
 *   RETURN: Either 0 for error or pointer to added list member
 **************************************************************************************************/

SAVEDS ASM struct FTPdStats *adv_confunc(REG(a0) struct Hook *hook,REG(a2) APTR mem_pool, REG(a1) struct FTPdStats *fs)
	{
	struct FTPdStats *entry;

	entry=AllocPooled(mem_pool,sizeof(struct FTPdStats));
	if(entry)
		{
		CopyMem(fs,entry,sizeof(struct FTPdStats));
		return(entry);
		}
	else return(0);
	}

/**************************************************************************************************
 * FUNCTION: adv_desfunc()
 *  PURPOSE: Hook function for NewList objects - Destructor
 *    INPUT: hook			=> Pointer to hook structure
 *           mem_pool	=> Global memory pool pointer
 *           line			=> One member of list to free
 *   RETURN: 0
 **************************************************************************************************/

SAVEDS ASM long adv_desfunc(REG(a0) struct Hook *hook,REG(a2) APTR mem_pool,REG(a1) struct FTPdStats *line)
	{
	if(line)
		{
		FreePooled(mem_pool,line,sizeof(struct FTPdStats));
		}
	return(0);
	}

/**************************************************************************************************
 * FUNCTION: adv_dspfunc()
 *  PURPOSE: Hook function for NewList objects - Display hook
 *    INPUT: hook		=> Pointer to hook structure
 *           array	=> Array where to add the entries
 *           line		=> One member to add or NULL for initial rendering of titles
 *   RETURN: 0
 **************************************************************************************************/

char unum[32],conf[32];

SAVEDS ASM long adv_dspfunc(REG(a0) struct Hook *hook,REG(a2) char **array,REG(a1) struct FTPdStats *line)
	{
	if(line)
		{
		*unum=*conf='\0';

		if(!line->UserNumber) FAMEStrCopy("N/A",unum,5);
		else
			{
			GetUserName(line->UserNumber,unum);
			}
		if(!line->CurrentConf) FAMEStrCopy("/",conf,5);
		else
			{
			GetConfName(line->CurrentConf,conf);
			}
		*array++=line->TaskAddress;
		*array++=line->ConnectTime;
		*array++=unum;
		*array++=conf;
		*array=line->ActionString;
		}
	else
		{
		*array++="\33bTaskAddress";
		*array++="\33bConnect time";
		*array++="\33bUser";
		*array++="\33bConf";
		*array="\33bCurrent action";
		}
	return(0);
	}

/**************************************************************************************************
 * FUNCTION: CreatePopObject()
 *  PURPOSE: Creates a popup (File) object
 *    INPUT: num		=> Unique number for this object
 *           key		=> Key shortcut to use
 *           help		=> Bubble-Help text
 *   RETURN: APTR		=> Pointer to object or NULL in case of failure
 **************************************************************************************************/

APTR CreatePopObject(long num,char key,char *help)
	{
	return(	PopaslObject,
						MUIA_Popstring_String, KeyString(0,106,key),
						MUIA_Popstring_Button, PopButton(MUII_PopFile),
						MUIA_Popasl_StartHook,&PrefsPopupHook,
						MUIA_UserData,num,
						MUIA_ExportID,num,
						MUIA_HelpNode,help,
						MUIA_Weight, 200,
					End);
	}

/**************************************************************************************************
 * FUNCTION: CreateKeyButton()
 *  PURPOSE: Creates a button object
 *    INPUT: label	=> Labeltext of button
 *           key		=> Key shortcut to use
 *           help		=> Bubble-Help text
 *   RETURN: APTR		=> Pointer to object or NULL in case of failure
 **************************************************************************************************/

APTR CreateKeyButton(char *label, char key,char *help)
	{
	return(	TextObject,
						ButtonFrame,
						MUIA_Text_Contents,label,
						MUIA_Text_PreParse,"\33c",
						MUIA_Text_HiChar,key,
						MUIA_ControlChar,key,
						MUIA_InputMode,MUIV_InputMode_RelVerify,
						MUIA_Background,MUII_ButtonBack,
						MUIA_HelpNode,help,
					End);
	}

/**************************************************************************************************
 * FUNCTION: CreateKeyCheckMark()
 *  PURPOSE: Creates a checkmark object
 *    INPUT: status	=> Initial status of checkmark
 *           key		=> Key shortcut to use
 *           help		=> Bubble-Help text
 *   RETURN: APTR		=> Pointer to object or NULL in case of failure
 **************************************************************************************************/

APTR CreateKeyCheckMark(BOOL status, char key, char *help)
	{
	return(	ImageObject,
						ImageButtonFrame,
						MUIA_InputMode,MUIV_InputMode_Toggle,
						MUIA_Image_Spec,MUII_CheckMark,
						MUIA_Image_FontMatch,TRUE,
						MUIA_Selected,status,
						MUIA_Background,MUII_ButtonBack,
						MUIA_ShowSelState,FALSE,
						MUIA_ControlChar,key,
						MUIA_HelpNode, help,
					End);
	}

/**************************************************************************************************
 * FUNCTION: CreateInteger()
 *  PURPOSE: Creates an integer object
 *    INPUT: key			=> Key shortcut to use
 *           weight 	=> Weight Factor for rendering
 *           maxlen 	=> Maximum of characters allowed
 *           contents => Default value for gadget
 *           help			=> Bubble-Help text
 *   RETURN: APTR			=> Pointer to object or NULL in case of failure
 **************************************************************************************************/


APTR CreateInteger(char key,long weight,long maxlen,long contents,char *help)
	{
	return(StringObject,
						StringFrame,
						MUIA_ControlChar,key,
						MUIA_Weight,weight,
						MUIA_String_Accept,"0123456789",
						MUIA_String_MaxLen,maxlen,
						MUIA_String_Integer,contents,
						MUIA_HelpNode,help,
				End);
	}

/**************************************************************************************************
 * FUNCTION: fail()
 *  PURPOSE: General failure function. Uses EasyRequest for warning infos if app is not loaded yet
 *    INPUT: app			=> Application Pointer
 *           *str			=> Error string
 *					 withexit	=> TRUE = Exit after displaying message. FALSE = do not exit application
 *   RETURN: none
 **************************************************************************************************/

VOID fail(APTR thisapp,char *str,BOOL noexit)
	{
  if (*str)
  	{
		if(thisapp) MUI_Request(app, mainwindow, 0,"FAME-FTPdCFG Warning:","Okay!","\33c%s", str);
		else
			{
			struct EasyStruct MyES=
				{
				sizeof(struct EasyStruct),
				0,
				NULL,
         "%s",
         "Okay",
				};
     	EasyRequest(NULL,&MyES,NULL,str);
			}
  	}
	if(noexit==FALSE) return;
	if (app) 						MUI_DisposeObject(app);
	if(fib)							FreeDosObject(DOS_FIB,fib); fib = NULL;
	if(mem_pool)      	DeletePool(mem_pool); mem_pool = NULL;
  if (MUIMasterBase) 	CloseLibrary(MUIMasterBase); MUIMasterBase=NULL;
	if (UtilityBase) 		CloseLibrary(UtilityBase); UtilityBase=NULL;
	if (FAMEBase) 			CloseLibrary((struct Library *)FAMEBase);	FAMEBase=NULL;
	if(IntuitionBase)		CloseLibrary((struct Library *)IntuitionBase); IntuitionBase = NULL;
  exit(0);
	}

/**************************************************************************************************
 * FUNCTION: ShowAbout()
 *  PURPOSE: Opens the well-known About dialog :)
 *    INPUT: none
 *   RETURN: none
 **************************************************************************************************/

STATIC void ShowAbout(void)
	{
	APTR titeltext,titeltxt2,mypic;
	long	status;

	SubWindow, aboutwin=WindowObject,
		MUIA_Window_Title			,	"About FAME-FTPdCFG",
		MUIA_Window_RefWindow	, mainwindow,
		MUIA_Window_NoMenus		,	TRUE,
		MUIA_Window_SizeGadget, FALSE,
		MUIA_Background   		, MUII_BACKGROUND,
		WindowContents, VGroup,TextFrame,
			Child, VSpace(3),
				Child, titeltext=TextObject,End,
				Child, HGroup,
					Child, ColGroup(3),
						Child, HSpace(0),
						Child, mypic=BodychunkObject,MUIA_Bodychunk_Body,BODY_inno_Data,MUIA_Bodychunk_Depth,BODY_inno_Depth,MUIA_Bodychunk_Masking,BODY_inno_Masking,MUIA_Bodychunk_Compression,BODY_inno_Compression,MUIA_Bitmap_SourceColors,BODY_inno_Colors,MUIA_Bitmap_Transparent,BODY_inno_Transparent,MUIA_Bitmap_Width,BODY_inno_Width,MUIA_Bitmap_Height,BODY_inno_Height,MUIA_FixHeight,BODY_inno_Height,MUIA_FixWidth,BODY_inno_Width,End,
						Child, HSpace(0),
					End,
				End,
				Child, titeltxt2=TextObject,End,
			End,
		End;
	if(!aboutwin) fail(app, "Cannot open about window!",TRUE);
	DoMethod(aboutwin,MUIM_Notify,MUIA_Window_CloseRequest,TRUE,app,2,MUIM_Application_ReturnID,ID_ABOUTCLOSE);
	DoMethod(titeltext,MUIM_SetAsString,MUIA_Text_Contents,"\33c\33bFAME-FTPdCFG V%s\33n Build %ld (%s)\n(c) 2003,2004 by Sascha 'SieGeL/tRSi' Pfalz of",COMPILE_VERSION,COMPILE_BUILD,COMPILE_DATE);
	DoMethod(titeltxt2,MUIM_SetAsString,MUIA_Text_Contents,"\033cThis Application configures the FAME FTP Server\n\n\033i\033uGreetings must go to:\033n\n\nAndreas 'Bysis' Lorenz for helping with TCP/IP problems\n\n\033i\033uBetatester:\033n\n\ndIGIMAN/tRSi, aRGON/tRSi, eXON, Elvin");
	DoMethod(app, OM_ADDMEMBER, aboutwin);
  set(app,MUIA_Application_Sleep,TRUE);
 	set(aboutwin,MUIA_Window_Open,TRUE);
	get(aboutwin,MUIA_Window_Open,&status);
	if(!status) fail(app,"Cannot open about window!",TRUE);
	}

/**************************************************************************************************
 * FUNCTION: RemoveAboutWin()
 *  PURPOSE: Closes the About window
 *    INPUT: none
 *   RETURN: none
 **************************************************************************************************/

STATIC void RemoveAboutWin(void)
	{
	set(aboutwin,MUIA_Window_Open,FALSE);
	DoMethod(app,OM_REMMEMBER, aboutwin);
	MUI_DisposeObject(aboutwin);aboutwin=NULL;
  set(app,MUIA_Application_Sleep,FALSE);
	}

/**************************************************************************************************
 * FUNCTION: DoubleStart()
 *  PURPOSE: Avoids doublestarting of the application
 *    INPUT: none
 *   RETURN: none
 **************************************************************************************************/

STATIC void DoubleStart(void)
	{

	DoMethod(mainwindow,MUIM_Window_ToFront);
	set(mainwindow,MUIA_Window_Activate,TRUE);
	}

/**************************************************************************************************
 * FUNCTION: SavePrefs()
 *  PURPOSE: Saves the preferences
 *    INPUT: none
 *   RETURN: none
 **************************************************************************************************/

STATIC void SavePrefs(void)
	{
	BPTR	fp;
	long	rc;
	char	ioerr[80],buffer[256];
	ULONG	x;
	int		p_lo,p_hi;

	// Retrieve data from all Gadgets and set preferences accordingly:

	FAMEFillMem(fconfig,0,sizeof(struct FTPdConfig));

	// Retrieve string gadgets:

	get(howto			,	MUIA_String_Contents,&x);	FAMEStrCopy((char *)x,fconfig->HowToUploadFile,255);
	get(uldir			, MUIA_String_Contents,&x);	FAMEStrCopy((char *)x,fconfig->UploadTempPath,255);
	get(ipfile		, MUIA_String_Contents,&x);	FAMEStrCopy((char *)x,fconfig->IPFile,255);
	get(titlefile	, MUIA_String_Contents,&x);	FAMEStrCopy((char *)x,fconfig->TitleFile,255);
	get(logdir		,	MUIA_String_Contents,&x);	FAMEStrCopy((char *)x,fconfig->LogFile,255);

	// Retrieve integer gadgets:

	get(portlo		, MUIA_String_Contents,&x);	p_lo = FAMEAtol((char *)x);
	get(porthi		, MUIA_String_Contents,&x);	p_hi = FAMEAtol((char *)x);
	get(maxusers	, MUIA_String_Contents,&x);	fconfig->MaxUsers = FAMEAtol((char *)x);

	// Perform some sanity checking for passive port ranges:

	if(p_lo > 0 && p_hi > 0)
		{
		if(p_lo >= p_hi)
			{
			SPrintf(buffer,"Passive port range invalid!\nStart value (%ld) must be lower than end value (%ld)!",p_lo,p_hi);
			fail(app,buffer,FALSE);
			return;
			}
		if(((p_hi - 5) <= 0) || ((p_hi - 5) < p_lo))
			{
      fail(app,"Port range is too small!!\nRange MUST be at least 5 numbers!\nSaving aborted!\n",FALSE);
			return;
			}
		fconfig->PortLo = p_lo;
		fconfig->PortHi = p_hi;
		}
	else		// Both values must be set to have effect, else we use automatic port assignment
  	{
    fconfig->PortLo = 0;
		fconfig->PortHi = 0;
		}

	// Retrieve checkmarks:

	get(debugwin,	MUIA_Selected,&x);	if(x) fconfig->Flags1|=CFG_USEDEBUG;
	get(usedns,		MUIA_Selected,&x);	if(x) fconfig->Flags1|=CFG_USEDNS;
	get(weektop,	MUIA_Selected,&x);	if(x) fconfig->Flags1|=CFG_WEEKTOP;

	// Retrieve numeric gadgets:

	get(fnamelength, MUIA_Numeric_Value,&x);	fconfig->FileNameLength = x;
	get(fbufsize, 	 MUIA_Numeric_Value,&x);	fconfig->FileBufferSize = x;
	get(timeout, 	 	MUIA_Numeric_Value,&x);		fconfig->Timeout 				= x*60;	// Have to save in seconds

	if(fp = Open(prefspath,MODE_NEWFILE))
		{
		rc = Write(fp,fconfig,sizeof(struct FTPdConfig));
		Close(fp);
		if(rc != sizeof(struct FTPdConfig))
			{
      fail(app,"Saved data seems to be broken or not saved correctly!",FALSE);
			}
		}
	else
		{
		Fault(IoErr(),NULL,ioerr,80);
		SPrintf(buffer,"SavePrefs failed: %s",ioerr);
    fail(app,buffer,FALSE);
		}
	}

/**************************************************************************************************
 * FUNCTION: LoadPrefs()
 *  PURPOSE: Loads the preferences. If file not found we use the cleared struct
 *    INPUT: none
 *   RETURN: none
 **************************************************************************************************/

STATIC void LoadPrefs(void)
	{
	BPTR	fp;
	char	buffer[256];

	if(fp = Open(prefspath,MODE_OLDFILE))
		{
		ExamineFH(fp,fib);
		if(fib->fib_Size != sizeof(struct FTPdConfig))
			{
			SPrintf(buffer,"Your Preferences file is outdated!\nYour prefs file is %ld bytes, but it has to be %ld bytes.\nPlease configure all options and SAVE the preferences\nto save the latest version.",fib->fib_Size,sizeof(struct FTPdConfig));
			fail(NULL,buffer,FALSE);
			}
		Read(fp,fconfig,sizeof(struct FTPdConfig));
		Close(fp);
		}
	if(!fconfig->FileNameLength) 	fconfig->FileNameLength = 12;
	if(!fconfig->Timeout)         fconfig->Timeout = 600;				// Defaults to 10 minutes
	}

/**************************************************************************************************
 * FUNCTION: UpdateStatDisplay()
 *  PURPOSE: Refreshes the StatsDisplay (TrackMonitor)
 *    INPUT: none
 *   RETURN: none
 **************************************************************************************************/

STATIC void UpdateStatDisplay(void)
	{
	struct	FTPdStats	*mydata;
  long	cnt = 0,myentries,i;
	BPTR	fp;
	static	char noconns[]="Currently there are no active FTP connections";

	DoMethod(adv_lv, MUIM_List_Clear);
	while(cnt < STATS_FILE_RETRY)
		{
		if(!(fp = Open(statname,MODE_OLDFILE)))		// File is accessed by many instances!
			{
			if(IoErr()==ERROR_OBJECT_NOT_FOUND)			// IoErr() = 205, this is okay and not treated as error
				{
				DoMethod(adv_txt,MUIM_SetAsString,MUIA_Text_Contents,"\033c%s",noconns);
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
			Close(fp);
			DoMethod(adv_txt,MUIM_SetAsString,MUIA_Text_Contents,"\033c%s",noconns);
			return;
			}
		if(!(mydata = AllocPooled(mem_pool,sizeof(struct FTPdStats) * (myentries+1))))
			{
			Close(fp);
    	fail(NULL,"Cannot allocate memory for statistics??",FALSE);
			return;
			}
    for(i = 0; i < myentries; i++)
			{
      if(!Read(fp,&mydata[i],sizeof(struct FTPdStats)))
				{
				Close(fp);
        fail(NULL,"Error reading statsfile",FALSE);
				return;
				}
			}
		Close(fp);
		for(i = 0; i < myentries; i++)
			{
			DoMethod(adv_lv,MUIM_List_InsertSingle,&mydata[i],MUIV_List_Insert_Sorted);
			}
		FreePooled(mem_pool,mydata,sizeof(struct FTPdStats) * (myentries+1));
		DoMethod(adv_txt,MUIM_SetAsString,MUIA_Text_Contents,"\033cCurrently there are %ld active connection(s)",myentries);
		return;
		}
	}

/**************************************************************************************************
 * FUNCTION: GetUserName()
 *  PURPOSE: Retrieves the Username and copy it to the supplied buffer
 *    INPUT: userid => Userid to retrieve
 *           buffer => Name will be copied here (32 bytes!)
 *   RETURN: none
 **************************************************************************************************/

void GetUserName(long userid, char *buffer)
	{
  struct	FAMEConfigRequest *fcr;
  struct	FAMEUserKeys			*myuserdata;
	long		myresult=0;

  fcr = AllocPooled(mem_pool,sizeof(struct FAMEConfigRequest));
	if(!fcr)
		{
    fail(NULL,"Unable to allocate fcr struct!",FALSE);
 		return;
		}
	FAMERequestReset(fcr);
	fcr->fcr_CfgUserTask 		= FindTask(NULL);
	fcr->fcr_CfgUserName 		= "FAME-FTPdCFG";
	fcr->fcr_CfgUserDesc 		= "FAME-FTPdCFG Editor";
  fcr->fcr_ConfigType  		= FCDD_UserKeys;					// We want the FAME user.data
	fcr->fcr_ConfigMode  		= FAMECFG_MODE_READ; 			// Nothing will be changed here!
  fcr->fcr_ConfigNum 			= userid;									// Search for the Username in user.data
	myuserdata = FAMEObtainConfig(fcr,&myresult);
	if(myuserdata)
		{
    FAMEStrCopy(myuserdata->UserName,buffer,31);
    }
	else
		{
		FAMEStrCopy("N/A",buffer,31);
		}
	FAMEReleaseConfig( myuserdata, FAMECFG_MODE_READ, FindTask( NULL ));
	FreePooled(mem_pool,fcr,sizeof(struct FAMEConfigRequest));
	}

/**************************************************************************************************
 * FUNCTION: GetConfName()
 *  PURPOSE: Retrieves the conference name and copy it to the supplied buffer
 *    INPUT: confid => Conference number to retrieve
 *           buffer => Name will be copied here (32 bytes!)
 *   RETURN: none
 **************************************************************************************************/

void GetConfName(long confid, char *buffer)
	{
  struct	FAMEConfigRequest *fcr;
  struct	FAMEConf					*myconf;
	long		myresult=0;

  fcr = AllocPooled(mem_pool,sizeof(struct FAMEConfigRequest));
	if(!fcr)
		{
    fail(NULL,"Unable to allocate fcr struct!",FALSE);
 		return;
		}
	FAMERequestReset(fcr);
	fcr->fcr_CfgUserTask 		= FindTask(NULL);
	fcr->fcr_CfgUserName 		= "FAME-FTPdCFG";
	fcr->fcr_CfgUserDesc 		= "FAME-FTPdCFG Editor";
  fcr->fcr_ConfigType  		= FCDD_Conf;							// We want the FAME confname
	fcr->fcr_ConfigMode  		= FAMECFG_MODE_READ; 			// Nothing will be changed here!
  fcr->fcr_ConfigNum 			= confid;
	myconf = FAMEObtainConfig(fcr,&myresult);
	if(myconf)
		{
    FAMEStrCopy(myconf->ConfName,buffer,31);
    }
	else
		{
		FAMEStrCopy("N/A",buffer,31);
		}
	FAMEReleaseConfig( myconf, FAMECFG_MODE_READ, FindTask( NULL ));
	FreePooled(mem_pool,fcr,sizeof(struct FAMEConfigRequest));
	}
