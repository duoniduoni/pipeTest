/**
 *  @file JExecution.h
 *
 *  @brief  execute a command line
 *
 *  copyright 2011, by JoStudio
 *
 *
 */
#ifndef _JExecutionH_
#define _JExecutionH_

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* exit code  when user abort */
#define EXIT_USER_ABORT -2


/*---------------------------------------------------------------------------*/
/** Call back function when stdin input
 *  @param [in] buf    buffer of data 
 *  @param [in] p_size   pointer to size of data
 *  @note  To set input data, you may copy data to buf, and set the size.
 */
typedef void FuncIn(char *buf,int *p_size);
/*---------------------------------------------------------------------------*/
/** Call back function when stdout,stderr output
 *  @param [in] buf    buffer of data
 *  @param [in] size   size of data
 *  @note  To get output data, you may copy n bytes data from buf, according to the size parameter.
 */
typedef void FuncOut(char *buf,int size);
/*---------------------------------------------------------------------------*/
/** Call back function when processing
 *  @param [in] p_abort   pointer to an integer specified whether abort the process
 *  @note  this function is called periodically during processing.
 *         To abort process, set abort to true
 */
typedef void FuncProcessing(int *p_abort);

/*---------------------------------------------------------------------------*/
/*   High level functions
/*---------------------------------------------------------------------------*/
/** Excute a command line, return immediately
 *  @param [in] szCommandLine     specifies the command line to execute. 
 *  @param [in] ShowWindow       Specifies how the window is to be shown. See Win32 API ShowWindow()
 *  @return     returns >0 if succeed.  return 0 if failed.
 */
int ExecCommandNoWait(char *szCommandLine,unsigned short ShowWindow);
/*---------------------------------------------------------------------------*/
/** Excute a command line, return until the process finished
 *  @param [in] szCommandLine     specifies the command line to execute.
 *  @param [in] ShowWindow       Specifies how the window is to be shown. See Win32 API ShowWindow()
 *  @param [in] OnProcessing     Call back function when processing, you may set abort to true to stop the process
 *  @return     returns the exit code of process of command line. return -1 if failed.
 */
int ExecCommandWait(char *szCommandLine,unsigned short ShowWindow,FuncProcessing *OnProcessing);
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
 *  @param [in] ShowWindow       Specifies how the window is to be shown. See Win32 API ShowWindow()
 *  @return     returns the exit code of the process of command line. return -1 if failed.
 */
int ExecCommandOutput(char *szCommandLine,char *Environment,unsigned short ShowWindow,
                       FuncOut *OnLineOut,FuncProcessing *OnProcessing);
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
 *  @return     returns the exit code of the process of command line.  return -1 if failed.
 */
int ExecCommandEx(char *szCommandLine,char *CurrentDirectory,char *Environment,unsigned short ShowWindow,
              FuncOut *OnStdOut,FuncProcessing *OnProcessing,FuncIn *OnStdIn);


/*---------------------------------------------------------------------------*/
/* structure of execution data */
typedef struct {
  void * hStdInRead;
  void * hStdInWrite;     ///< handles for stdin
  void * hStdOutRead;     ///< handles for stdout
  void * hStdOutWrite;
  void * hStdErrWrite;    ///< handles for stderr

  char * in_buffer;       ///< buffer of input
  char * out_buffer;      ///< buffer of output
  FuncIn *OnStdIn;        ///< Call back function when stdin need input
  FuncOut *OnStdOut;      ///< Call back function when stdout is something out
  FuncProcessing *OnProcessing; ///< Call back function when processing
  FuncOut *OnLineOut;      ///< Call back function when on line is out

  char *CommandLine;       ///< Command line
  char *Environment;       ///< Environment , See Win32 API CreateProcess() for detail
  char *CurrentDirectory;  ///< CurrentDirectory
  unsigned short ShowWindow; ///< Specifies how the window is to be shown, See. Win32 API ShowWindow()

  int abort;               ///< set this flag to abort execution
  int wait;                ///< Specifies whether wait for the procession complete, default=1=true
  unsigned int out_buffer_index;    ///< Specifies used byte of out_buffer,  used in OnLineOut    
}EXEC_INFO;

/*---------------------------------------------------------------------------*/
/*   Low level functions
/*---------------------------------------------------------------------------*/
/** Create a EXEC_INFO data structure and initalize it, malloc() memory for its buffer
 *  @return   pointer to EXEC_INFO data structure created.
 */
EXEC_INFO * ExecCreate(void);
/*---------------------------------------------------------------------------*/
/** Execute
 *  @param [in]  exec_info pointer to EXEC_INFO data structure specified the task
 *  @return     returns the exit code of the process of command line.   return -1 if failed.
 */
int ExecExec(EXEC_INFO *exec_info);
/*---------------------------------------------------------------------------*/
/** Free the EXEC_INFO data structure and memory of its buffer */
void ExecFree(EXEC_INFO *exec_info);
/*---------------------------------------------------------------------------*/


#ifdef __cplusplus
}
#endif /* __cplusplus */
#endif  /* _JExecutionH_ */
