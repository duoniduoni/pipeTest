// pipeTest.cpp : �������̨Ӧ�ó������ڵ㡣
//

#include "stdafx.h"
#include "JExecution.h"

//����һ����������Ļص�����
void OnLineOut(char *buf,int size)
{
	printf("%s\n", buf);
}

int _tmain(int argc, _TCHAR* argv[])
{
	int n;
	char *command;

	command = "E:\\tools\\adb.exe devices ";   //����������˼���ǵ���DOS��ִ��dir�����ʾ��ǰĿǰ�µ��ļ�
	n=ExecCommandOutput(command,NULL,SW_HIDE,OnLineOut,NULL);
	printf("<Return:>%d",n);

	scanf("%d", &n);

	return 0;
}

