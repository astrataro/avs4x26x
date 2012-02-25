#include "avs4x264mod.h"

HANDLE  hConsole = 0;
int color_printf(char *fmt, ...)
{
	int rt;
	if (hConsole == 0)
		hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
	va_list args;
	va_start(args, fmt);

	SetConsoleTextAttribute(hConsole, 0xe);
	rt = vfprintf(stderr, fmt, args);
	/* TODO: print log into log-file */
	va_end(args);
	SetConsoleTextAttribute(hConsole, 7);
	fflush( stderr );
	return rt;
}

struct timeval time_start = {0};
int title_printf(char *fmt, ...)
{
	int rt;
	char title[1024], *szbuffer;
	struct timeval time_end;
	gettimeofday(&time_end, NULL);
	int ms = (time_end.tv_sec - time_start.tv_sec) * 1000000 + time_end.tv_usec - time_start.tv_usec;
	if (ms < 0)
		time_start = time_end;
/*	else if(ms < 10000)
		return 0;*/
	
	GetConsoleTitle(title, 1024);
	title[256] = 0; /* prevent buffer overflow */
	for (szbuffer = title; szbuffer[1] != '$' && szbuffer[0] != 0; szbuffer++)
		;
	
	if(ms < 333334 && szbuffer[1] == '$')
		return 0;
	
	strcpy(szbuffer, " $ ");
	szbuffer += 3;
	
	va_list args;
	va_start(args, fmt);
	rt = vsprintf(szbuffer, fmt, args);
	SetConsoleTitle(title);
	va_end(args);
	time_start = time_end;
	return rt;
}
