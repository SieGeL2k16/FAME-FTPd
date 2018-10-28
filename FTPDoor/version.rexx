/*************************************************************
 * version.rexx by Sascha 'SieGeL' Pfalz                     *
 *                                                           *
 * Updates and/or creates version.h file                     *
 *                                                           *
 * Supported Parameters:                                     *
 *                                                           *
 *  NONE     -> Update only Compile Date                     *
 *  VERSION  -> Increment Version. Also sets Revision to 0   *
 *  REVISION -> Increment Revision. Version won't be changed *
 *                                                           *
 * $VER: version.rexx 0.1 (05.05.2001)                       *
 *                                                           *
 * 0.2 (09.06.2003) - Added COMPILE_BUILD TAG                *
 * 0.1 (05.05.2001) - Initial version                        *
 *************************************************************/

verfile = 'version.h'

compiledate    = "#define COMPILE_DATE "
compilever     = "#define COMPILE_VERSION "
build          = "#define COMPILE_BUILD "

/********************************************
 * Please do not modify anything else below *
 ********************************************/

options results

compileversion = compilever
mydate = DATE(S)
myday  = SUBSTR(mydate,7,2)
mymon  = SUBSTR(mydate,5,2)
myyear = SUBSTR(mydate,1,4)
currentdate = myday"."mymon"."myyear
compiledate = compiledate||d2c(34)currentdate||d2c(34)
IF WORD(ARG(1),1)='VERSION' THEN DO
  updateversion  = 1
  updaterevision = 0
END
ELSE IF WORD(ARG(1),1)='REVISION' THEN DO
  updateversion  = 0
  updaterevision = 1
END
ELSE DO
  updateversion  = 0
  updaterevision = 0
END
IF ~OPEN(fd,verfile,'r') THEN DO
  compileversion = compileversion||d2c(34)||"0.1"||d2c(34)
	newbuild = build||'1'
END
ELSE DO
	READLN(fd)
  compileversion = READLN(fd)
	buildold = READLN(fd)
	CLOSE(fd)
  oldvertag = SUBSTR(compileversion,LENGTH(compilever)+2,LENGTH(compileversion)-LENGTH(compilever)-2)
  oldver    = SUBSTR(oldvertag,1,INDEX(oldvertag,'.')-1)
	oldrev    = SUBSTR(oldvertag,INDEX(oldvertag,'.')+1)
	oldbuild  = SUBSTR(buildold,LENGTH(build))
  IF updateversion = 1 THEN DO
		ver = oldver + 1
    rev = 0
  END
  ELSE IF updaterevision = 1 THEN DO
    ver = oldver
    rev = oldrev + 1
		IF rev >=999 THEN DO
      rev = 0
      ver = ver + 1
		END
  END
	ELSE DO
		ver = oldver
    rev = oldrev
  END
  compileversion = compilever||d2c(34)||ver||"."||rev||d2c(34)
	oldbuild = oldbuild + 1
	newbuild = build||oldbuild
END
IF ~OPEN(fd,verfile,'w') THEN DO
  SAY "Cannot open "verfile" for writing!!!"
  EXIT
END
ELSE
  WRITELN(fd,compiledate)
  WRITELN(fd,compileversion)
	WRITELN(fd,newbuild)
  CLOSE(fd)
EXIT
