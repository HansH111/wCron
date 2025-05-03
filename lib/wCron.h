// wcron.h
#ifndef _WCRON_H_
#define _WCRON_H_

#pragma warning(disable: 4996)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <io.h>
#include <time.h>
#include <shellapi.h>
#include <sys/stat.h>
#include "dynarray.h"

// Struct definitions
typedef struct {
    int low;
    int high;
    int step;
} ranges;

typedef struct {
    int m1, m2; // minute range
    int h1, h2; // hour
    int d1, d2; // day
    int M1, M2; // month
    int w1, w2; // weekday
    char *cmd;
    char *param;
    char *cmdstr;
    HANDLE hproc;
    DWORD pid;
    time_t stim;
    DWORD hash;
    int log;
} cronentry;

typedef struct {
    char *ext;
    char *fn;
    char *opts;
} t_cmdext;

typedef struct {
    char *fn;
    struct stat stat;
} t_cronfile;

// Global variables
extern char			 *pExeName;
extern char			 *pDirName;
extern char			 pBaseName[81];
extern char			 pServiceName[81];
extern char			 pLogName[MAX_PATH];
extern char			 pTabName[MAX_PATH];
extern char			 pCmdfn[MAX_PATH];
extern int			 LOGmode;
extern char			 LOGinfodir[256];
extern char			 shutdowncmd[256];
extern char			 startupcmd[256];
extern int			 noexec;
extern int			 noerr;
extern int			 noact;
extern time_t		 now;
extern struct tm	 *st;
extern dynamic_array entry;
extern dynamic_array range;
extern dynamic_array cmdext;
extern dynamic_array crons;
extern int			 maxcron;
extern int			 maxrange;
extern int		   	 CONSOLEmode;

extern SERVICE_TABLE_ENTRY   DispatchTable [];
extern SERVICE_STATUS        ServiceStatus;
extern SERVICE_STATUS_HANDLE hServiceStatusHandle;

extern volatile int	StopServing;
extern volatile int	StoppedServing;

// main
void			LOGprintf (const char *fmt, ...);
void			ReportError (char *FunctionName, long ErrorCode);
int				DetermineNames(void);
int				Texists(char *fn, int tdir);

// wcron_service
VOID WINAPI		ServiceMain(DWORD dwArgc, LPTSTR *lpszArgv);
DWORD WINAPI	ServiceHandler(DWORD fdwControl, DWORD dwEventType, LPVOID lpEventData, LPVOID lpContext);
VOID			ServiceInstall(void);
VOID			ServiceUnInstall(void);
BOOL			ServiceStart(void);
BOOL			ServiceStop(void);
void			Svc_start();
void			Svc_stop();
int				Svc_status();
BOOL			ServiceShutdown();

// wCron_core
int				Texists(char *fn, int tdir);
DWORD			calchash(unsigned char *str);
char			*make_cmdstr(char *cmd, char *params);
void			add_cmdext(char *ext, char *fn, char *opts);
void			clr_cmdext(void);
void			add_cronfile(char *fn);
void			del_cronfile(char *fn);
void			init_cronfile(char *fn);
void			read_cronentries(void);
void			reload_crontab(void);
int				ExecProcess(int i, char *hashstr, char *cmd, char *params, char *outfn, int waitmsec);
DWORD			doshellexecwait(char *cmdstr, char *paramstr, char *id, DWORD waitmsec);
void			check_cronjobs(void);
void			exe_cronjobs(void);
void			doinit(void);
void			doinit_service(void);
DWORD WINAPI	MyService(void);

#endif