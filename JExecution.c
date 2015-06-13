#include <windows.h>
#include <stdio.h>

#include "JExecution.h"
/*---------------------------------------------------------------------------*/
#define BUFSIZE 4096
/*---------------------------------------------------------------------------*/
EXEC_INFO * ExecCreate(void)
{
  EXEC_INFO *exec_info;
  exec_info=malloc(sizeof(EXEC_INFO));
  
  exec_info->hStdInRead = NULL;
  exec_info->hStdInWrite = NULL;     ///< handles for stdin
  exec_info->hStdOutRead = NULL;     ///< handles for stdout
  exec_info->hStdOutWrite = NULL;
  exec_info->hStdErrWrite = NULL;    ///< handles for stderr

  exec_info->in_buffer = malloc(BUFSIZE);       ///< buffer of input
  exec_info->out_buffer = malloc(BUFSIZE);      ///< buffer of output
  exec_info->OnStdIn = NULL;        ///< Call back function when stdin need input
  exec_info->OnStdOut = NULL;      ///< Call back function when stdout is something out
  exec_info->OnProcessing = NULL; ///< Call back function when processing
  exec_info->OnLineOut = NULL;    ///< Call back function when stdout line out

  exec_info->CommandLine = NULL;       ///< Command line
  exec_info->Environment = NULL;       ///< Environment
  exec_info->CurrentDirectory = NULL;  ///< CurrentDirectory
  exec_info->ShowWindow = SW_HIDE;     ///< Specifies how the window is hide
  exec_info->abort = FALSE;
  exec_info->wait = TRUE;   ///< wait for procession complete
  return exec_info;
}
/*---------------------------------------------------------------------------*/
void ExecFree(EXEC_INFO *exec_info)
{
  free(exec_info->in_buffer);
  free(exec_info->out_buffer);
  free(exec_info);
}

/*---------------------------------------------------------------------------*/
BOOL WriteToPipe(EXEC_INFO *e)
{
  int dwSize;
  DWORD dwWritten;
  BOOL bSuccess = FALSE;

  if (e->OnStdIn==NULL) return TRUE;

  (*(e->OnStdIn))(e->in_buffer,&dwSize);
  if (dwSize==0) return TRUE;
       
  bSuccess = WriteFile( e->hStdInWrite, e->in_buffer, dwSize, &dwWritten, NULL);
  return bSuccess;
}
/*---------------------------------------------------------------------------*/
/// call back functin when stdout , used internal to create OnStdLineOut
void StdOutToLineOut(EXEC_INFO *e)
{
  unsigned int i,from,size;

  if (e==NULL) return;
  if (e->out_buffer_index==0) return;
  if (e->OnLineOut==NULL) return;

  from=0; size=0; i=0;
  
  //scan e->out_buffer
  while (i<e->out_buffer_index)
    {
      //if current char is \n
      if (e->out_buffer[i]=='\n')
        {
           //set current char to 0, calculate the size
           e->out_buffer[i]=0;  size = i-from;
           //if there is a \r char before current char ,set it to 0 too
           if ( (i>0) && (e->out_buffer[i-1]=='\r'))
                {
                  e->out_buffer[i-1]=0; //eat char of \r
                  size --;
                }
           // printf("%s=%d\n",e->out_buffer+from,size);
           //call the call back function
           (*(e->OnLineOut))(e->out_buffer+from,size);
           from = i+1;
        }
      //next char
      i++;
    }

  //move the buffer,skip the chars ouputed
  if (from>0)
  {
    memmove(e->out_buffer,e->out_buffer+from,e->out_buffer_index-from);
    e->out_buffer[e->out_buffer_index-from]=0;
    e->out_buffer_index  -=from;
  }
}
/*---------------------------------------------------------------------------*/
/// Read output from pipe for STDOUT,STDERR
BOOL ReadFromPipe(EXEC_INFO *e)
{
  DWORD dwRead;
  BOOL bSuccess = FALSE;

  if ( (e->OnStdOut==NULL) && (e->OnLineOut==NULL)) return TRUE;

  if (e->OnLineOut!=NULL)
    {  //if it is line output
       bSuccess = ReadFile( e->hStdOutRead, e->out_buffer+e->out_buffer_index,
                            BUFSIZE-e->out_buffer_index, &dwRead, NULL);
       if ((bSuccess) && (dwRead!=0))
         {  e->out_buffer_index += dwRead;
            e->out_buffer[e->out_buffer_index] = 0; // pad with 0 in case of error
            StdOutToLineOut(e);
         }
    }
  else
    { //if not line output
      bSuccess = ReadFile( e->hStdOutRead, e->out_buffer, BUFSIZE, &dwRead, NULL);
      if ((bSuccess) && (dwRead!=0))
          if (e->OnStdOut!=NULL)
             {
                e->out_buffer[dwRead] = 0; // pad with 0 in case of error
                (*(e->OnStdOut))(e->out_buffer,dwRead);
             }
    }
  return bSuccess;
}
/*---------------------------------------------------------------------------*/
int ExecExec(EXEC_INFO *e)
{
  SECURITY_ATTRIBUTES saAttr;

  PROCESS_INFORMATION piProcInfo;
  STARTUPINFO siStartInfo;
  BOOL bSuccess = FALSE;
  int result;
  DWORD process_exit_code;
  HANDLE hStdInWriteTmp,hStdOutReadTmp;
  MSG message;

  // if no command line, exit
  if (e->CommandLine == NULL) return 0;

  result = -1;
  e->abort = 0;
  e->out_buffer_index = 0;
  hStdInWriteTmp = hStdOutReadTmp = NULL;

  // Set the bInheritHandle flag so pipe handles are inherited.
  saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
  saAttr.bInheritHandle = TRUE;
  saAttr.lpSecurityDescriptor = NULL;

  // Create a pipe for stdin
  if (e->OnStdIn!=NULL)
    if (!CreatePipe(&(e->hStdInRead), &hStdInWriteTmp,&saAttr, 0))
      goto QuitWithError;

  // Duplicate and create new  input write handle. Set
  // the inheritance properties to FALSE. Otherwise, the child inherits
  // the these handles; resulting in non-closeable handles to the pipes
  // being created.
  if (e->OnStdIn!=NULL)
    if (!DuplicateHandle(GetCurrentProcess(), hStdInWriteTmp, GetCurrentProcess(),
              &(e->hStdInWrite), 0, FALSE, DUPLICATE_SAME_ACCESS))
       goto QuitWithError;

  // Create a pipe for stdout
  if ((e->OnStdOut!=NULL) || (e->OnLineOut!=NULL))
    if (!CreatePipe(&hStdOutReadTmp, &(e->hStdOutWrite),&saAttr, 0))
      goto QuitWithError;

  // Duplicate hStdOutWrite to create handle for stderr
  if ((e->OnStdOut!=NULL) || (e->OnLineOut!=NULL))
    if (!DuplicateHandle(GetCurrentProcess(), e->hStdOutWrite, GetCurrentProcess(),
                 &(e->hStdErrWrite), 0, TRUE, DUPLICATE_SAME_ACCESS))
     goto QuitWithError;

  // Duplicate and create new output read handle. Set
  // the inheritance properties to FALSE. Otherwise, the child inherits
  // the these handles; resulting in non-closeable handles to the pipes
  // being created.
  if ((e->OnStdOut!=NULL) || (e->OnLineOut!=NULL))
    if (!DuplicateHandle(GetCurrentProcess(), hStdOutReadTmp, GetCurrentProcess(),
              &(e->hStdOutRead), 0, FALSE, DUPLICATE_SAME_ACCESS))
       goto QuitWithError;

  // Close hStdOutReadTmp,hStdInWriteTmp handle
  CloseHandle(hStdOutReadTmp); hStdOutReadTmp = NULL;
  CloseHandle(hStdInWriteTmp); hStdInWriteTmp = NULL;

  // Set up members of the PROCESS_INFORMATION structure.
  ZeroMemory( &piProcInfo, sizeof(PROCESS_INFORMATION) );

  // Set up members of the STARTUPINFO structure.
  // This structure specifies the STDIN and STDOUT handles for redirection.
  ZeroMemory( &siStartInfo, sizeof(STARTUPINFO) );
  siStartInfo.cb = sizeof(STARTUPINFO);
  siStartInfo.wShowWindow = e->ShowWindow;
  siStartInfo.dwFlags  |= STARTF_USESHOWWINDOW;
  if ( (e->hStdOutWrite!=NULL) && (e->hStdOutWrite!=NULL) && (e->hStdOutWrite!=NULL))
     siStartInfo.dwFlags  |= STARTF_USESTDHANDLES;
  siStartInfo.hStdOutput = (e->hStdOutWrite!=NULL) ? e->hStdOutWrite : GetStdHandle(STD_OUTPUT_HANDLE);
  siStartInfo.hStdError  = (e->hStdErrWrite!=NULL) ? e->hStdErrWrite : GetStdHandle(STD_ERROR_HANDLE);
  siStartInfo.hStdInput  = (e->hStdInRead!=NULL) ? e->hStdInRead : GetStdHandle(STD_INPUT_HANDLE);


  // Create the child process.
  bSuccess = CreateProcess(NULL,
      e->CommandLine, // command line
      NULL,          // process security attributes
      NULL,          // primary thread security attributes
      TRUE,          // handles are inherited
      0,             // creation flags
      NULL,          // use parent's environment
      NULL,          // use parent's current directory
      &siStartInfo,  // STARTUPINFO pointer
      &piProcInfo);  // receives PROCESS_INFORMATION

  // If an error occurs, exit the application.
  if (!bSuccess ) goto QuitWithError;

  //waits until the given process is waiting for user input with no input pending
  WaitForInputIdle(piProcInfo.hProcess,1000);

  // Close the write end of the pipe before reading from the
  // read end of the pipe, to control child process execution.
  // The pipe is assumed to have enough buffer space to hold the
  // data the child process has already written to it.
  CloseHandle(e->hStdInRead);    e->hStdInRead = NULL;
  CloseHandle(e->hStdOutWrite);  e->hStdOutWrite = NULL;
  CloseHandle(e->hStdErrWrite);  e->hStdErrWrite = NULL;

  //write to pipe
  WriteToPipe(e);

  if (e->wait)
  {
    // while the process not terminated or abort, read from pipe
    while (GetExitCodeProcess(piProcInfo.hProcess,&process_exit_code))
      {
         if (PeekMessage(&message, 0, 0, 0, PM_REMOVE))
           {
               TranslateMessage(&message);
               DispatchMessage(&message);              
           }
         ReadFromPipe(e);
         if (process_exit_code!=STILL_ACTIVE) break;
         if (e->OnProcessing!=NULL) (*(e->OnProcessing))(&(e->abort));
         if (e->abort)
            {
              TerminateProcess(piProcInfo.hProcess,-1);
              process_exit_code = EXIT_USER_ABORT;
              break;
            }
      }

    //when line ouput mode, if there is still chars in buffer
    if ( (e->OnLineOut!=NULL) && (e->out_buffer_index>0))
       {
          e->out_buffer[e->out_buffer_index++]='\n';  //padding a \n at the end of buffer
          e->out_buffer[e->out_buffer_index]=0;     
          StdOutToLineOut(e);
       }
    result = process_exit_code;
  }
  else
    result = (int )piProcInfo.hProcess;
   
  // close handle of process
  CloseHandle(piProcInfo.hProcess);
  CloseHandle(piProcInfo.hThread);

 QuitWithError:
  if (e->hStdInRead!=NULL)  { CloseHandle(e->hStdInRead);   e->hStdInRead = NULL;}
  if (hStdInWriteTmp!=NULL) { CloseHandle(hStdInWriteTmp);  hStdInWriteTmp = NULL;}
  if (e->hStdInWrite!=NULL) { CloseHandle(e->hStdInWrite);  e->hStdInWrite = NULL;}
  if (e->hStdOutRead!=NULL) { CloseHandle(e->hStdOutRead);  e->hStdOutRead = NULL;}
  if (hStdOutReadTmp!=NULL) { CloseHandle(hStdOutReadTmp);  hStdOutReadTmp = NULL;}
  if (e->hStdOutWrite!=NULL){ CloseHandle(e->hStdOutWrite); e->hStdOutWrite = NULL;}
  if (e->hStdErrWrite!=NULL){ CloseHandle(e->hStdErrWrite); e->hStdErrWrite = NULL;}
  if (e->hStdOutRead!=NULL) { CloseHandle(e->hStdOutRead);  e->hStdOutRead = NULL;}

  if (!e->wait)
     if (result<0) result=0;
     
  return result;
}
/*---------------------------------------------------------------------------*/
/** Excute a command line with full parameters
 *  @param [in] szCommandLine     specifies the command line to execute.
 *  @param [in] CurrentDirectory  specifies the current drive and directory for the child process.
 *  @param [in] Environment      Points to an environment block for the new process. See Win32 API CreateProcess() for detail
                                 An environment block consists of a null-terminated block of null-terminated strings.  name=value etc.
                                 If this parameter is NULL, the new process uses the environment of the calling process.
                                 Note that an ANSI environment block is terminated by two zero bytes;
 *  @param [in] ShowWindow       Specifies how the window is to be shown. See Win32 API ShowWindow()
 *  @param [in] OnStdOut         Call back function when stdout output
 *  @param [in] OnProcessing     Call back function when processing, you may set abort to true to stop the process
 *  @param [in] OnStdIn          Call back function where stdin input
 *  @return     returns the exit code of the process of command line.
 */
int ExecCommandEx(char *szCommandLine,char *CurrentDirectory,char *Environment,unsigned short ShowWindow,
              FuncOut *OnStdOut,FuncProcessing *OnProcessing,FuncIn *OnStdIn)
{
  EXEC_INFO *e;
  int result;

  //create EXEC_INFO
  e = ExecCreate();

  //set parameters
  e->CommandLine=szCommandLine;
  e->CurrentDirectory=CurrentDirectory;
  e->Environment=Environment;
  e->ShowWindow=ShowWindow;
  e->OnStdIn=OnStdIn;
  e->OnStdOut=OnStdOut;
  e->OnProcessing=OnProcessing;

  //execute
  result = ExecExec(e);

  //free
  ExecFree(e);

  //return
  return result;
}
/*---------------------------------------------------------------------------*/
/** Excute a command line, return until the process finished
 *  @param [in] szCommandLine     specifies the command line to execute.
 *  @param [in] Environment      Points to an environment block for the new process. See Win32 API CreateProcess() for detail
                                 An environment block consists of a null-terminated block of null-terminated strings.  name=value etc.
                                 If this parameter is NULL, the new process uses the environment of the calling process.
                                 Note that an ANSI environment block is terminated by two zero bytes;
 *  @param [in] ShowWindow       Specifies how the window is to be shown. See Win32 API ShowWindow()
 *  @param [in] OnLineOut        Call back function when stdout,stderr has one line of output
 *  @param [in] OnProcessing     Call back function when processing, you may set abort to true to stop the process
 *  @param [in] ShowWindow       Specifies how the window is to be shown. See Win32 API ShowWindow() for detail
 *  @return     returns the exit code of the process of command line.
 */
int ExecCommandOutput(char *szCommandLine,char *Environment,unsigned short ShowWindow,
                       FuncOut *OnLineOut,FuncProcessing *OnProcessing)
{
  EXEC_INFO *e;
  int result;

  //create EXEC_INFO
  e = ExecCreate();

  //set parameters
  e->CommandLine=szCommandLine;
  e->Environment=Environment;
  e->ShowWindow=ShowWindow;
  e->OnLineOut=OnLineOut;
  e->OnProcessing=OnProcessing;

  //execute
  result = ExecExec(e);

  //free
  ExecFree(e);

  //return
  return result;
}
/*---------------------------------------------------------------------------*/
/** Excute a command line, return immediately
 *  @param [in] szCommandLine     specifies the command line to execute. 
 *  @param [in] ShowWindow       Specifies how the window is to be shown. See Win32 API ShowWindow() for detail
 *  @return     returns >0 if succeed.  return 0 if failed.
 */
int ExecCommandNoWait(char *szCommandLine,unsigned short ShowWindow)
{
  EXEC_INFO *e;
  int result;

  //create EXEC_INFO
  e = ExecCreate();

  //set parameters
  e->CommandLine=szCommandLine;
  e->ShowWindow = ShowWindow;
  e->wait = FALSE;
  
  //execute
  result = ExecExec(e);

  //free
  ExecFree(e);

  //return
  return result;
}
/*---------------------------------------------------------------------------*/
/** Excute a command line, return until the process finished
 *  @param [in] szCommandLine     specifies the command line to execute.
 *  @param [in] ShowWindow       Specifies how the window is to be shown. See Win32 API ShowWindow()
 *  @param [in] OnProcessing     Call back function when processing, you may set abort to true to stop the process
 *  @return     returns the exit code of process of command line. return -1 if failed.
 */
int ExecCommandWait(char *szCommandLine,unsigned short ShowWindow,FuncProcessing *OnProcessing)
{
  EXEC_INFO *e;
  int result;

  //create EXEC_INFO
  e = ExecCreate();

  //set parameters
  e->CommandLine=szCommandLine;
  e->ShowWindow = ShowWindow;
  e->OnProcessing = OnProcessing;
  
  //execute
  result = ExecExec(e);

  //free
  ExecFree(e);

  //return
  return result;
}

