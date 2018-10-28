# FAME-FTPd - FTP Daemon for Amiga F.A.M.E. BBS systems

This is a RFC959 compliant FTP daemon for F.A.M.E. with an external MUI based
configuration tool.

## Requirements to compile this source

This Sourcecode can be compiled with SAS/C 6.5 compiler on AmigaOS 3.x.

The following dependencies must be met:

- AmiTCP-SDK-4.3.lha must be installed and the following assigns have to be set:
  - NETINCLUDE: points to netinclude/ subdirectory of SDK
  - NETLIB: points to netlib/ subdirectory of SDK

- FAME SDK must be installed

- To compile the Config Tool you need the MUI SDK

I've tested the compile runs for all three tools without any problems on WinUAE.

The Original Documentation from the release is also included.


## How to compile

In root directory all you have to do is enter "smake", the code will be compiled as
68040 compatible binary called "FAME-FTPd". This is the main server component.

Next, switch to the "Config" directory and again enter "smake". This will compile
the external MUI driven configuration tool called "FAME-FTPdCFG".

Finally switch to the "FTPDoor" directory and enter "smake". This will compile the
companion door for FAME called "FTPDoor.FIM".


## Final words

This is the last 68k code I've developed for AmigaOS 3.x. All of the code is now
GNU licensed. Do what you want with it but keep the original authors intact.

If you just want to download the original release please follow this link:

https://www.saschapfalz.de/downloads_readme.php?FID=36&CAT=0


Have fun!

SieGeL/tRSi
