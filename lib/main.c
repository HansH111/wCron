// wCron  :  a unix style cron service for windows
//
#include "wcron.h"

#define     VERSION	"4.05"

char		*pExeName=0;
char		*pDirName=0;
char		pBaseName[81]="wCron";
char		pServiceName[81] = "wCron";

char		pCmdfn   [MAX_PATH];
char		pLogName [MAX_PATH];
char		pTabName [MAX_PATH];

int			LOGlast_mon=-1;
int			LOGlast_mday=0;
int			CONSOLEmode=0;						// service mode

// ============ Some general functions ================
// log everything in <exename>_log.txt
void LOGprintf (const char *fmt, ...)
{
    time_t     now;
    struct tm  *t;
    FILE       *fp;

	if (LOGmode<=0) return;
    now=time(0);
    t=localtime(&now);
	if (CONSOLEmode)
		fp=stdout;
	else {
		int sw=0;
		if (LOGlast_mday) {
  	   	    if (LOGmode>2 && t->tm_mday != LOGlast_mday)  sw=1;
		    if (t->tm_mon != LOGlast_mon)  sw=1;
		}
		if (sw) {
			char tfn[MAX_PATH];
			sprintf(tfn,"%s.prev.txt",pLogName);
			if (access(tfn,0)==0)      unlink(tfn);
			if (access(pLogName,0)==0) rename(pLogName,tfn);
		}
	    fp = fopen(pLogName,"a");
	}
    if(fp != NULL) {
        va_list va_alist;
        if(!fmt) { return; }
		LOGlast_mon=t->tm_mon;
		LOGlast_mday=t->tm_mday;
        fprintf(fp,"%04d-%02d-%02d %02d:%02d:%02d ",
            1900+t->tm_year,t->tm_mon+1,t->tm_mday,t->tm_hour,t->tm_min,t->tm_sec);
        va_start (va_alist, fmt);
        vfprintf(fp,fmt,va_alist);
        va_end (va_alist);
        if (!CONSOLEmode) fclose(fp);
    }
}

// Get error message from windows
void ReportError (char *FunctionName, long ErrorCode)
{
    LPVOID lpMsgBuf;

    FormatMessage (FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM, NULL, ErrorCode, MAKELANGID (LANG_NEUTRAL, SUBLANG_DEFAULT), (LPTSTR) &lpMsgBuf, 0, NULL);
    LOGprintf("%s error %i %s\n", FunctionName, ErrorCode, lpMsgBuf);
    LocalFree( lpMsgBuf );
}

void Syntax(void)
{
	printf("Syntax : %s [-install] [-remove] [-console]\n",pBaseName);
	printf("         Unix style cron service for Windows, version %s\n\n",VERSION);
	printf("Options: -install   install as service and start\n");
	printf("         -remove    stop service and uninstall\n");
	printf("         -stop      stop service\n");
	printf("         -start     start service\n");
	printf("         -status    current status\n");
	printf("         -version   show version only\n");
	printf("         -console   run as console (for debugging)\n");
	printf("         -testmode  run as console without executing\n");
	printf("         -entries   show crontab entries\n");
	printf("\n");
	printf("Multiple cron services can be running by renaming the exe\n");
	printf("\nCrontab definition support for :\n");
	printf("\nSettings in front of line:\n");
	printf("    @log=[0-9]    loglevel 0=none 1=some 2=errors 3=log output.... 9=very, default 3\n");
	printf("    @cronfile=<additional crontab filename>\n");
	printf("    @cmdext=<extension>;<exe filename>[;<options>]\n");
    printf("    @shutdowncmd=<shutdown script>\n");
    printf("    @startupcmd=<startup script>\n");
	printf("\nTime specifications:\n");
	printf("    *         =  every minute/hour/... \n");
	printf("    ?         =  replaced by startup minute/hour/... \n");
	printf("    0-23      =  range, happens on 0,1,2, ....\n");
	printf("    1-23/4    =  range with step, happens on 1,5,9, ....\n");
	printf("    */4       =  range with step, happens on 0,4,8, ....\n");
	printf("    0,3-6/2,9 =  multiple, happens on 0,3,5,9\n");
	printf("\nOptional 1st character before command:\n");
	printf("    ~         =  disable logging and debug information\n");
	printf("    -         =  some log information\n");
	printf("    =         =  more log information\n");
	printf("    +         =  enable debug information\n");
	exit(1);
}

// -- return 1 if exists, 0 if not found
int Texists(char *fn, int tdir)
{
    DWORD attr;
	attr = GetFileAttributes (fn);
	if (tdir) {
  	  if (attr==INVALID_FILE_ATTRIBUTES)  return 0;
	  if (attr&FILE_ATTRIBUTE_DIRECTORY)  return 1;
	} else {
	  if (attr != 0xFFFFFFFF && (! (attr & FILE_ATTRIBUTE_DIRECTORY)) ) return 1;
	}
	return 0;
}

int DetermineNames(void)
{    
	char *ptr, ffn[MAX_PATH];
    int  siz;

    siz=GetModuleFileName(NULL, ffn, MAX_PATH);
    ffn[siz] = 0;
	pExeName=(char *)malloc(strlen(ffn)+1);
	if (pExeName==NULL) return(1);
    strcpy(pExeName, ffn);
	pDirName=(char *)malloc(strlen(ffn)+1);
	if (pDirName==NULL) return(2);
    strcpy(pDirName,ffn);
	ptr=strrchr(pDirName,'\\');   // BaseDIR, should always work	
	if (ptr) {
		ptr[0]=0;				  // end dirname
		if (strlen(&ptr[1])<80) {
	        strcpy(pBaseName,&ptr[1]);
			pBaseName[strlen(pBaseName)-4]=0;		// strip .exe
		}
	}
    strcpy(pServiceName,pBaseName);
    sprintf(pLogName,"%s\\%s_log.txt",pDirName,pBaseName);
    sprintf(pTabName,"%s\\%s.tab"    ,pDirName,pBaseName);
	// set current directory
	SetCurrentDirectory(pDirName);

	sprintf(pCmdfn,"%s\\Cmd.exe",pDirName); 
    if (Texists(pCmdfn,0)==0) strcpy(pCmdfn,"C:\\Windows\\System32\\Cmd.exe");
	return(0);
}

// startup routine
int  main (int argc, char *argv [])
{
	if (DetermineNames()!=0) {
		if (pDirName) free(pDirName);
		if (pExeName) free(pExeName);
		return(1);
	}
    // initialize service table
    DispatchTable [0].lpServiceName = (LPSTR)&pServiceName[0];
    DispatchTable [0].lpServiceProc = ServiceMain;

    // install or uninstall or start service
    if (argc == 2) {
		if (stricmp(argv[1],"-install")==0)  {
			CONSOLEmode=1;
			ServiceInstall();
		} else if (stricmp(argv[1],"-remove")==0) {
			CONSOLEmode=1;
			if (Svc_status() == 1 ) {
				Svc_stop();	
			}
			ServiceUnInstall();
		} else if (stricmp(argv[1],"-start")==0) {
			int stat;
			CONSOLEmode=1;
			stat= Svc_status();
			if (stat == -1 ) {
				ServiceInstall();
			} else if (stat == 0) {
		  	    Svc_start();
			} else {
				LOGprintf("Service %s already started\n",pServiceName);
			}
		} else if (stricmp(argv[1],"-shutdown")==0) {
			CONSOLEmode=1;
			ServiceShutdown();
		} else if (stricmp(argv[1],"-stop")==0) {
			CONSOLEmode=1;
			if (Svc_status() == 1 ) {
				Svc_stop();	
			} else {
				LOGprintf("Service %s already stopped\n",pServiceName);
			}
		} else if (stricmp(argv[1],"-status")==0) {
			CONSOLEmode=1;
			Svc_status();	
		} else if (stricmp(argv[1],"-console") == 0)  {
			CONSOLEmode=1;
			LOGmode=4;
			StopServing=0;
			MyService();			// console mode
        } else if (stricmp(argv[1],"-testmode") == 0)  {
			CONSOLEmode=2;
			LOGmode=4;
			StopServing=0;
			MyService();			// console mode
        } else if (stricmp(argv[1],"-entries") == 0)  {
			CONSOLEmode=1;
			LOGmode=-1;
			doinit();
			read_cronentries();
        } else if (stricmp(argv[1],"-version") == 0)  {
			printf("%s version %s\n",pBaseName,VERSION);
		} else
			Syntax();
    } else {
        if (!StartServiceCtrlDispatcher(DispatchTable))
            ReportError("StartServiceCtrlDispatcher", GetLastError() );
    }
	if (pDirName) free(pDirName);
	if (pExeName) free(pExeName);
	return 0;
}

