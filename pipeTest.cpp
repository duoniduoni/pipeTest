// pipeTest.cpp : 定义控制台应用程序的入口点。
//

#include "stdafx.h"
#include "JExecution.h"

//定义一个处理输出的回调函数
void OnLineOut(char *buf,int size)
{
	printf("%s\n", buf);
}

int _tmain(int argc, _TCHAR* argv[])
{
	int n;
	char *command;

	command = "E:\\tools\\adb.exe devices ";   //这个命令的意思就是调用DOS，执行dir命令，显示当前目前下的文件
	n=ExecCommandOutput(command,NULL,SW_HIDE,OnLineOut,NULL);
	printf("<Return:>%d",n);

	scanf("%d", &n);

	return 0;
}

