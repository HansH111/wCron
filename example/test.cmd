@ECHO OFF
:: ---------------------------------------------------------------------------
:: Program       : test.cmd
:: Author        : Hans Harder
:: Description   : Dummy cron program
:: ---------------------------------------------------------------------------
SETLOCAL
  set ProgramDir=%~dp0
  set ProgramName=%~n0
:: ================= Commandline processing start =================
  echo program=%0  info=%~n0  ext=%~x0
  
  if "%1" EQU "" (
     call :SYNTAX
     set /P CMDLINE=Cmdline: %ProgramName% 
  ) else (
     set CMDLINE=%*
  )
  if "%CMDLINE%" EQU ""   goto ENDok
  echo %time%;---;%ProgramName% cmdline=%CMDLINE%
  
  echo %time%;---;Waiting approx. 5 seconds
  PING localhost -n 5 >nul
  goto ENDok

:SYNTAX
  echo.
  echo Syntax : %ProgramName% parameters
  echo          Dummy program to simulate cron actions
  echo.
  goto :EOF

:ENDok
  echo %time%;---;Done.
  if "%1" EQU ""  set /P dummy=Press return to end
  ENDLOCAL
  exit /B 0
::EOF 
 