// wCron  :  a unix style cron service for windows
//
#include "wcron.h"
#include <winsvc.h>
#include <stdio.h>
#include <time.h>

int volatile	StopServing = 1;
int volatile	StoppedServing = 1;

SERVICE_TABLE_ENTRY   DispatchTable [] = {{NULL, NULL}, {NULL, NULL}};
SERVICE_STATUS        ServiceStatus;
SERVICE_STATUS_HANDLE hServiceStatusHandle;

// --------------------------------------------- installation routines ---------------------------------------------------

// installs and runs the service
VOID ServiceInstall()
{
    SC_HANDLE schSCManager = OpenSCManager(NULL, NULL, SC_MANAGER_CREATE_SERVICE);
    if (!schSCManager)
        ReportError("OpenSCManager",GetLastError() );
    else {
        SC_HANDLE schService = CreateService(schSCManager, pServiceName, pServiceName, SERVICE_ALL_ACCESS, SERVICE_WIN32_OWN_PROCESS|SERVICE_INTERACTIVE_PROCESS, SERVICE_AUTO_START, SERVICE_ERROR_NORMAL, pExeName, NULL, NULL, NULL, NULL, NULL);
        if (!schService)
            ReportError("CreateService", GetLastError ());
        else {
			char str[256];
			SERVICE_DESCRIPTION sd;
            LOGprintf("Service %s installed\n",pServiceName);
 	 	    // Change the service description.
			sprintf(str,"%s Unix like cron service",pServiceName);
		    sd.lpDescription = str;
		    if( !ChangeServiceConfig2(schService, SERVICE_CONFIG_DESCRIPTION, &sd) ) {
		        LOGprintf("ChangeServiceConfig2 failed\n");
			} else {
				LOGprintf("Service description updated successfully.\n");
			}
			if (!StartService(schService, 0, NULL))
                ReportError("StartService", GetLastError ());
            CloseServiceHandle(schService);
        }
        CloseServiceHandle(schSCManager);
    }
}

// stops and uninstalls the service
void Svc_stop()
{
    SC_HANDLE schSCManager = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
    if (!schSCManager)
        ReportError("OpenSCManager", GetLastError() );
    else {
        SC_HANDLE schService = OpenService(schSCManager, pServiceName, SERVICE_ALL_ACCESS);
        if (!schService)
            ReportError("OpenService", GetLastError() );
        else {
            if (!ControlService(schService, SERVICE_CONTROL_STOP, &ServiceStatus))
                ReportError("ControlService", GetLastError() );
            else
                LOGprintf("Service %s stopped\n", pServiceName);
            CloseServiceHandle(schService);
        }
        CloseServiceHandle(schSCManager);
    }
}

// stops and uninstalls the service
void Svc_start()
{
    SC_HANDLE schSCManager = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
    if (!schSCManager)
        ReportError("OpenSCManager", GetLastError() );
    else {
        SC_HANDLE schService = OpenService(schSCManager, pServiceName, SERVICE_ALL_ACCESS);
        if (!schService)
            ReportError("OpenService", GetLastError() );
        else {
            if (!StartService(schService, 0, NULL))
                ReportError("StartService", GetLastError ());
            else
                LOGprintf("Service %s started\n", pServiceName);
            CloseServiceHandle(schService);
        }
        CloseServiceHandle(schSCManager);
    }
}

int Svc_status()
{
  SC_HANDLE      schService;
  SC_HANDLE      schSCManager;
  SERVICE_STATUS ssStatus; 
  int stat;

  stat=-2;
  schSCManager = OpenSCManager( NULL,                     // machine (NULL == local)
                                NULL,                     // database (NULL == default)
                                SC_MANAGER_ALL_ACCESS );  // access required
  if ( schSCManager )
  {
    schService = OpenService(schSCManager, pServiceName, SERVICE_ALL_ACCESS);
    if (schService)
    {
        QueryServiceStatus( schService, &ssStatus );
		switch (ssStatus.dwCurrentState) {
			case SERVICE_STOP_PENDING  :
				LOGprintf("%s stop pending.\n", pServiceName );
				break;
			case SERVICE_START_PENDING :
				LOGprintf("%s start pending.\n", pServiceName );
				break;
			case SERVICE_STOPPED : 
				LOGprintf("%s stopped.\n", pServiceName );
                stat=0;
				break;
			case SERVICE_RUNNING : 
				LOGprintf("%s running.\n", pServiceName );
				stat=1;
				break;
			default : 
				LOGprintf("%s unknown state %d.\n", pServiceName, ssStatus.dwCurrentState);
				break;
		}
        CloseServiceHandle(schService);
	} else {
		stat=-1;
		LOGprintf("%s not installed.\n", pServiceName );
	}
    CloseServiceHandle(schSCManager);
  } else
    ReportError("OpenSCManager", GetLastError() );
  return(stat);
}

// stops and uninstalls the service
void ServiceUnInstall()
{
    SC_HANDLE schSCManager = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
    if (!schSCManager)
        ReportError("OpenSCManager", GetLastError() );
    else {
        SC_HANDLE schService = OpenService(schSCManager, pServiceName, SERVICE_ALL_ACCESS);
        if (!schService)
            ReportError("OpenService", GetLastError() );
        else {
            if (!DeleteService(schService))
                ReportError("DeleteService", GetLastError() );
            else
                LOGprintf("Service %s removed\n",pServiceName);
            CloseServiceHandle(schService);
        }
        CloseServiceHandle(schSCManager);
    }
}

// --------------------------------------------- service handling routines ---------------------------------------------------

VOID WINAPI ServiceMain(DWORD dwArgc, LPTSTR *lpszArgv)
{
    DWORD status = 0;
    DWORD specificError = 0xfffffff;

	DetermineNames();
    LOGprintf("ServiceMain [%s] log=%s base=%s\n",pServiceName, pLogName,pBaseName);
	// DEFAULTS for ServiceStatus commands
    ServiceStatus.dwServiceType = SERVICE_WIN32;
    ServiceStatus.dwControlsAccepted = SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_PRESHUTDOWN | SERVICE_ACCEPT_SHUTDOWN | SERVICE_ACCEPT_POWEREVENT; // | SERVICE_ACCEPT_PAUSE_CONTINUE;
    ServiceStatus.dwWin32ExitCode = 0;
    ServiceStatus.dwServiceSpecificExitCode = 0;
    ServiceStatus.dwCheckPoint = 0;
    ServiceStatus.dwWaitHint = 0;

	hServiceStatusHandle = RegisterServiceCtrlHandlerExA(pServiceName, ServiceHandler, NULL);
    if (!hServiceStatusHandle) {
        ReportError("RegisterServiceCtrlHandler",GetLastError() );
        return;
    }
    LOGprintf("ServiceMain register ok\n");

    // end of initialization - set status
    ServiceStatus.dwCurrentState = SERVICE_RUNNING;
    ServiceStatus.dwCheckPoint = 0;
    ServiceStatus.dwWaitHint = 0;
	if(!SetServiceStatus(hServiceStatusHandle, &ServiceStatus)) {
        ReportError("SetServiceStatus", GetLastError() );
	}
    LOGprintf("ServiceMain start\n");

	// start serving
    if (!ServiceStart() )  LOGprintf("Service started\n");
}

DWORD WINAPI ServiceHandler(DWORD fdwControl, DWORD dwEventType, LPVOID lpEventData, LPVOID lpContext)
{
   switch (fdwControl) {
        case SERVICE_CONTROL_STOP:
            LOGprintf("Servicehandler %x stop\n",fdwControl);
			ServiceStatus.dwWin32ExitCode = 0;
            ServiceStatus.dwCurrentState = SERVICE_STOP_PENDING;
			ServiceStatus.dwControlsAccepted = 0;
			ServiceStatus.dwCheckPoint++;
            ServiceStatus.dwWaitHint = 16000;
            if (!SetServiceStatus (hServiceStatusHandle, &ServiceStatus))
                ReportError("SetServiceStatus", GetLastError() );

			Svc_status();
			ServiceStop();

			ServiceStatus.dwWin32ExitCode = 0;
            ServiceStatus.dwCurrentState = SERVICE_STOPPED;
            ServiceStatus.dwCheckPoint++;
            ServiceStatus.dwWaitHint = 0;
            if (!SetServiceStatus (hServiceStatusHandle, &ServiceStatus))
                ReportError("SetServiceStatus", GetLastError() );
			Svc_status();
            break;

		case SERVICE_CONTROL_POWEREVENT:
            LOGprintf("Servicehandler %x powerevent stopserving=%d  StoppedServing=%d\n",
 			  	       fdwControl,StopServing,StoppedServing);
			Svc_status();
			break;

		case SERVICE_CONTROL_PRESHUTDOWN:
		case SERVICE_CONTROL_SHUTDOWN:
            if (fdwControl != SERVICE_CONTROL_POWEREVENT) LOGprintf("Servicehandler %x shutdown\n",fdwControl);
 	  		ServiceStatus.dwWin32ExitCode = 0;
            ServiceStatus.dwCurrentState = SERVICE_STOP_PENDING;
			ServiceStatus.dwControlsAccepted = 0;
			ServiceStatus.dwCheckPoint = 0;
            ServiceStatus.dwWaitHint = 16000;
            if (!SetServiceStatus (hServiceStatusHandle, &ServiceStatus))
                ReportError("SetServiceStatus", GetLastError() );
			
			Svc_status();
			ServiceShutdown();
            
			ServiceStatus.dwWin32ExitCode = 0;
            ServiceStatus.dwCurrentState = SERVICE_STOPPED;
            ServiceStatus.dwCheckPoint = 0;
            ServiceStatus.dwWaitHint = 0;
			if (!SetServiceStatus (hServiceStatusHandle, &ServiceStatus))
                ReportError("SetServiceStatus", GetLastError() );
			Svc_status();
            break;

        case SERVICE_CONTROL_CONTINUE:
            LOGprintf("Servicehandler %x continue\n",fdwControl);
            ServiceStatus.dwWin32ExitCode = 0;
            ServiceStatus.dwCurrentState = SERVICE_RUNNING;
            ServiceStatus.dwCheckPoint = 0;
            ServiceStatus.dwWaitHint = 0;

            // start serving
            ServiceStart();

            if (!SetServiceStatus (hServiceStatusHandle, &ServiceStatus))
                ReportError("SetServiceStatus", GetLastError() );
            break;

        case SERVICE_CONTROL_INTERROGATE:
            if (!SetServiceStatus (hServiceStatusHandle, &ServiceStatus))
                ReportError("SetServiceStatus", GetLastError() );
            break;
	default:
        LOGprintf("Servicehandler %x not implemented\n",fdwControl);
		return ERROR_CALL_NOT_IMPLEMENTED;

   }
  return NO_ERROR;
}

// --------------------------------------------- service-specific routines ---------------------------------------------------
BOOL ServiceStart()
{
    DWORD ThreadId;

    LOGprintf("Starting .........\n");
    // wait if the service has to stop first
    while (StopServing) {
        if (StoppedServing)
            StopServing = 0;
        else
            Sleep(100);
    }

    // run service routine in its own thread
    if (!CreateThread(0, 0, (LPTHREAD_START_ROUTINE)MyService, 0, 0, &ThreadId)) {
        ReportError("ChreateThread", GetLastError() );
        return 0;
    } else
        return 1;
}

BOOL ServiceStop()
{
    LOGprintf("Stopping .........\n");
	LOGmode=3;
	if (shutdowncmd[0]) {
		LOGprintf("Shutdowncmd: %s\n",shutdowncmd);
		doshellexecwait(shutdowncmd, "stop","stop",15000);
	} else {
		LOGprintf("Stop skipped, no shutdowncmd defined\n");
	}
	if (CONSOLEmode == 0)  {
      StoppedServing = 0;
      StopServing = 1;
      noact=0;
      while (StopServing) {
          if (StoppedServing)
              StopServing = 0;
          else
              Sleep(100);
      }
	}
    return 1;
}

BOOL ServiceShutdown()
{
    if (CONSOLEmode) doinit_service();		// so shutdowncmd gets filled
	LOGmode=3;
    LOGprintf("Shutdown .......\n");
	if (shutdowncmd[0]) {
		LOGprintf("Shutdowncmd: %s\n",shutdowncmd);
		doshellexecwait(shutdowncmd,"stopall","shutdown",15000);
	} else {
		LOGprintf("Shutdown skipped, no shutdowncmd defined\n");
	}
	noact=0;
	if (CONSOLEmode == 0)  {
        StoppedServing = 0;
        StopServing = 1;
        while (StopServing) {
            if (StoppedServing)
                StopServing = 0;
            else
                Sleep(100);
        }
	}
    return 1;
}
