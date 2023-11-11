// main.c
//

#include <stdio.h>

#ifdef _WIN32
#include "windows.h"
#endif


#define LN_MAX_LEN   256
#define RD_BUFF_SIZE 2048
#define WR_BUFF_SIZE 2048


int port_overlapped = 1;

int port_open(char* port);
int port_close(void);
int port_read(char* pdata, int size);
int port_write(char* pdata, int size);

int help(void);


int main(int argc, char* argv[])
{
#ifdef TRACE_ARGS
	fprintf(stderr,"argc='%d'\n", argc);
	for (int i = 0; i < argc; i++)
		fprintf(stderr,"argv[%d]='%s'\n", i, argv[i]);
#endif
	// arg1 - port
	char port[128] = {0};
	if (argc > 1)
		strcpy_s(port, sizeof(port), (char*)argv[1]);
	else
	{
		fprintf(stderr, "Expected 1st argument (port).\n");
		return help();
	}
	// arg2 - cmd
	char cmd[2048] = {0};
	if (argc > 2)
		strcpy_s(cmd, sizeof(cmd), (char*)argv[2]);
	else
	{
		fprintf(stderr, "Expected 2nd argument (command).\n");
		return help();
	}
	// -b baudrate (default 115200)
	unsigned long baud = 115200;
	// -e ending line (default "ok")
	char end[2048] = "ok";
	// -t timeout (default 1000ms)
	unsigned long timeout = 1000;
	// -r read out before write (default 1)
	int read_out = 1;
	int argn = 3;
	while  (argn < argc)
	{
		if (strcmp(argv[argn], "-b") == 0)
		{
			sscanf_s(argv[argn+1], "%lu", &baud);
			argn += 2;
		}
		else if (strcmp(argv[argn], "-e") == 0)
		{
			strcpy_s(end, sizeof(end), (char*)argv[argn+1]);
			argn += 2;
		}
		else if (strcmp(argv[argn], "-t") == 0)
		{
			sscanf_s(argv[argn+1], "%lu", &timeout);
			argn += 2;
		}
		else if (strcmp(argv[argn], "-r") == 0)
		{
			sscanf_s(argv[argn+1], "%d", &read_out);
			argn += 2;
		}
		else if (strcmp(argv[argn], "-o") == 0)
		{
			sscanf_s(argv[argn+1], "%d", &port_overlapped);
			argn += 2;
		}
		else
		{
			fprintf(stderr, "Invalid argument '%s'.", port);
			return help();
		}
	}
	int cmdlen = strlen(cmd);
	if (cmd[cmdlen - 1] != '\n')
	{
		cmd[cmdlen] = '\n';
		cmd[++cmdlen] = 0;
	}

	char line[LN_MAX_LEN];
	int linelen = 0;
	char data[RD_BUFF_SIZE];

#if 1

	if (port_open(port) != 0)
	{
		fprintf(stderr, "Unable to open port '%s'.", port);
		return -2;
	}
	if (read_out)
		while (port_read(data, sizeof(data)) > 0);
	port_write(cmd, cmdlen);
	int readed = 0;
	int start_tick = GetTickCount();
	int run = 1;
	while (run)
	{
		readed = port_read(data, sizeof(data));
		for (int i = 0; i < readed; i++)
		{
			char ch = data[i];
			if (linelen >= LN_MAX_LEN)
			{
				port_close();
				return -3;
			}
			if (ch == '\n')
			{
				line[linelen] = 0;
				printf("%s\n", line);
				if (strcmp(line, end) == 0)
				{
					run = 0;
					break;
				}
				linelen = 0;
			}
			else
			{
				line[linelen] = ch;
				linelen++;
			}
		}
		if ((GetTickCount() - start_tick) >= timeout)
			run = 0;
	}
	port_close();
	return 0;
		
#else

	HANDLE handle = CreateFile(port, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (handle != INVALID_HANDLE_VALUE)
	{
		DCB dcb = {0};
		dcb.DCBlength = sizeof(DCB);
		r = GetCommState(handle, &dcb);
		dcb.fBinary = 0;
		dcb.BaudRate = 115200;
		dcb.ByteSize = 8;
		dcb.StopBits = ONESTOPBIT;
		dcb.Parity = NOPARITY;
		r = SetCommState(handle, &dcb);
		COMMTIMEOUTS tim = {0};
		r = GetCommTimeouts(handle, &tim);
		tim.ReadIntervalTimeout = 1000;
		tim.ReadTotalTimeoutMultiplier = 0;
		tim.ReadTotalTimeoutConstant = 1000;
		tim.WriteTotalTimeoutMultiplier = 0;
		tim.WriteTotalTimeoutConstant = 1000;
		r = SetCommTimeouts(handle, &tim);
		SetupComm(handle,2048,2048);
		DWORD rd = 0;
		char data[2048];
		memset(data, 0, sizeof(data));
		do { ReadFile(handle, data,2048,&rd,0); } while (rd);
		DWORD wr = 0;
		int len = strlen(cmd);
		if (cmd[len - 1] != '\n')
		{
			cmd[len] = '\n';
			cmd[len + 1] = 0;
		}
		r = WriteFile(handle, cmd,strlen(cmd),&wr,0);
	//	Sleep(100);
		int linelen = 0;
		char line[2048];
		DWORD tick_start = GetTickCount();
		int run = 1;
		while (run)
		{
			r = ReadFile(handle, data,2048,&rd,0);
			if (r & (rd > 0))
			for (DWORD i = 0; i < rd; i++)
			{
				char chr = data[i];
				if (chr == '\n')
				{
					line[linelen] = 0;
					linelen = 0;
					printf("%s\n", line);
					if (strlen(end) > 0)
					{
						if (strcmp(line, end) == 0)
						{
							run = 0;
//							fprintf(stderr,"end - ending line\n");
							break;
						}
					}
/*					else
					{
						run = 0;
//						fprintf(stderr,"end - single line\n");
					}*/
				}
				else
					line[linelen++] = chr;
			}	
			if ((GetTickCount() - tick_start) >= timeout)
			{
//				fprintf(stderr,"end - timeout\n");
				break;
			}
		}
		CloseHandle(handle);
		return 0;
	}
#endif

	return 0;
}



#ifdef _WIN32

HANDLE port_handle = 0;

int port_open(char* port)
{
	port_handle = CreateFile(port, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, ((port_overlapped)?FILE_FLAG_OVERLAPPED:0) | FILE_ATTRIBUTE_NORMAL, NULL);
	if (port_handle != INVALID_HANDLE_VALUE)
	{
		DCB dcb = {0};
		dcb.DCBlength = sizeof(DCB);
		if (!GetCommState(port_handle, &dcb))
		{
			CloseHandle(port_handle);
			return -2;
		}
		dcb.fBinary = 1;
		dcb.BaudRate = 115200;
		dcb.ByteSize = 8;
		dcb.StopBits = ONESTOPBIT;
		dcb.Parity = NOPARITY;
		dcb.fDtrControl = DTR_CONTROL_DISABLE;
		dcb.fRtsControl = RTS_CONTROL_DISABLE;
		if (!SetCommState(port_handle, &dcb))
		{
			CloseHandle(port_handle);
			return -3;
		}
		COMMTIMEOUTS tim = {0};
		if (!GetCommTimeouts(port_handle, &tim))
		{
			CloseHandle(port_handle);
			return -4;
		}
		tim.ReadIntervalTimeout = 1;
		tim.ReadTotalTimeoutMultiplier = 0;
		tim.ReadTotalTimeoutConstant = 1;
		tim.WriteTotalTimeoutMultiplier = 0;
		tim.WriteTotalTimeoutConstant = 1;
		if (!SetCommTimeouts(port_handle, &tim))
		{
			CloseHandle(port_handle);
			return -5;
		}
		if (!SetupComm(port_handle, RD_BUFF_SIZE, RD_BUFF_SIZE))
		{
			CloseHandle(port_handle);
			return -6;
		}
		//Sleep(100);
		return 0;
	}
	return -1;
}

int port_close(void)
{
	if ((port_handle != 0) && (port_handle != INVALID_HANDLE_VALUE))
	{
		CloseHandle(port_handle);
		port_handle = 0;
		return 0;
	}
	return -1;
}

int port_read(char* pdata, int size)
{
	DWORD readed = 0;
	if (port_overlapped)
	{
		OVERLAPPED overlapped = { 0 };
		if (!ReadFile(port_handle, pdata, size, &readed, &overlapped))
		{
			switch (GetLastError())
			{
			case NO_ERROR:
				break;
			case ERROR_IO_PENDING:
				if (GetOverlappedResult(port_handle, &overlapped, &readed, TRUE))
					break;
			default:
				CancelIoEx(port_handle, &overlapped);
				return -1;
			}
		}
	}
	else
		if (!ReadFile(port_handle, pdata, size, &readed, 0))
			return -1;
	return readed;
}

int port_write(char* pdata, int size)
{
	DWORD written = 0;
	if (port_overlapped)
	{
		OVERLAPPED overlapped = { 0 };
		if (!WriteFile(port_handle, pdata, size, &written, &overlapped))
		{
			switch (GetLastError())
			{
			case NO_ERROR:
				break;
			case ERROR_IO_PENDING:
				if (GetOverlappedResult(port_handle, &overlapped, &written, TRUE))
					break;
			default:
				CancelIoEx(port_handle, &overlapped);
				return -1;
			}
		}
	}
	else
		if (!WriteFile(port_handle, pdata, size, &written, 0))
			return -1;
	return written;
}


#endif // _WIN32


int help(void)
{
	printf("usage:\n");
	printf("\tportcmd port command [args]\n");
	printf("args:\n");
	printf("\t-b baudrate (default 115200)\n");
	printf("\t-e ending line (default 'ok')\n");
	printf("\t-t timeout (default 1000ms)\n");
	printf("\t-r read out before write (default 1)\n");
	return -1;
}
