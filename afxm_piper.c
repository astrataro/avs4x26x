#include "avs4x264mod.h"

extern avs_hnd_t avs_h;
buffer_t OBuffer = {0};


int _writefile(HANDLE hFile, char *lpBuffer, int nNumberOfBytesToWrite)
{
	DWORD temp;
	return WriteFile(hFile, lpBuffer, nNumberOfBytesToWrite, &temp, NULL);
}

int _mt_writefile(HANDLE hFile, char *lpBuffer, int nNumberOfBytesToWrite)
{
	if (OBuffer.size > OBuffer.capacity >> 1)
		usleep((OBuffer.size << 10) - 250000);
	while (OBuffer.size >= OBuffer.capacity)
	{
		usleep(500000);
	}
	node_t *lnk = OBuffer.dynarray + OBuffer.tail;
	lnk->data = lpBuffer;
	lnk->length = nNumberOfBytesToWrite;
	OBuffer.tail++;
	if (OBuffer.tail >= OBuffer.capacity)
		OBuffer.tail -= OBuffer.capacity;
	OBuffer.size++;
	return 1;
}

void __cdecl BufferedPipe(void *p)
{
	int result;
	DWORD temp;
	pipe_info_t *PipeInfo = (pipe_info_t *)p;
	SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);
	while (1)
	{
		while (OBuffer.size == 0)
		{
			//printf("~%d~", OBuffer.size);
			usleep(100000);
		}
		if (OBuffer.size < 0)
			return;
		node_t *clink = OBuffer.dynarray + OBuffer.head;
		if (clink->length > 0)
		{
			result = WriteFile(PipeInfo->h_pipeWrite, clink->data, clink->length, &temp, NULL);
			if (!result)
			{
				//printf("~%d\n", clink->length);
				//putchar('&');
				OBuffer.exitcode = -1;
				return;
			}
		}
		//printf("-%d*%d ", clink->length, OBuffer.head);
		free(clink->data);
		clink->data = 0;
		clink->length = 0;
		OBuffer.head++;
		if (OBuffer.head >= OBuffer.capacity)
			OBuffer.head -= OBuffer.capacity;
		OBuffer.size--;
	}
}

int CreateX264Process(char *cmd, cmd_t *cmdopts, video_info_t *VideoInfo, pipe_info_t *PipeInfo)
{
	//createprocess related
	//execute the commandline
	PipeInfo->h_process = GetCurrentProcess();
	PipeInfo->h_stdOut = GetStdHandle(STD_OUTPUT_HANDLE);
	PipeInfo->h_stdErr = GetStdHandle(STD_ERROR_HANDLE);

	if (PipeInfo->h_stdOut == INVALID_HANDLE_VALUE || PipeInfo->h_stdErr == INVALID_HANDLE_VALUE)
	{
		fprintf( stderr, "Error: Couldn\'t get standard handles!");
		return ERR_AVS_FAIL;
	}

	PipeInfo->saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
	PipeInfo->saAttr.bInheritHandle = TRUE;
	PipeInfo->saAttr.lpSecurityDescriptor = NULL;

	if (!CreatePipe(&PipeInfo->h_pipeRead, &PipeInfo->h_pipeWrite, &PipeInfo->saAttr, PIPE_BUFFER_SIZE))
	{
		fprintf( stderr, "Error: Pipe creation failed!");
		return ERR_AVS_FAIL;
	}

	if ( !SetHandleInformation(PipeInfo->h_pipeWrite, HANDLE_FLAG_INHERIT, 0) )
	{
		fprintf( stderr, "Error: SetHandleInformation");
		return ERR_PIPE_FAIL;
	}

	ZeroMemory( &PipeInfo->pi_info, sizeof(PROCESS_INFORMATION) );
	ZeroMemory( &PipeInfo->si_info, sizeof(STARTUPINFO) );
	PipeInfo->si_info.cb = sizeof(STARTUPINFO);
	PipeInfo->si_info.dwFlags = STARTF_USESTDHANDLES;
	PipeInfo->si_info.hStdInput = PipeInfo->h_pipeRead;
	PipeInfo->si_info.hStdOutput = PipeInfo->h_stdOut;
	PipeInfo->si_info.hStdError = PipeInfo->h_stdErr;

	if (!CreateProcess(NULL, cmd, NULL, NULL, TRUE, 0, NULL, NULL, &PipeInfo->si_info, &PipeInfo->pi_info))
	{
		fprintf( stderr, "Error: Failed to create process <%d>!", (int)GetLastError());
		free(cmd);
		return ERR_PIPE_FAIL;
	}

	//cleanup before writing to pipe
	CloseHandle(PipeInfo->h_pipeRead);
	free(cmd);

	if (cmdopts->x264Affinity)
		SetProcessAffinityMask(PipeInfo->pi_info.hProcess, cmdopts->x264Affinity);

	if (cmdopts->PipeMT)
	{
		PipeInfo->BufferSize = cmdopts->BufferSize;
		_beginthread(BufferedPipe, 0, (void *)PipeInfo);
	}
}

int WritePipeLoop(int *Func(HANDLE, char *, int), cmd_t *cmdopts, video_info_t *VideoInfo, pipe_info_t *PipeInfo)
{
	unsigned int frame, h_half, w_half;
	AVS_VideoFrame *frm;
	char *planeY, *planeU, *planeV;
	int j;
	//prepare for writing
	int buflength = VideoInfo->i_width * VideoInfo->i_height + h_half * w_half  * 2;
	char title[1024], szbuffer[256];
	if (PipeInfo->BufferSize == 0)
		/* Auto:
			>2M (~1920x1080 = 3.1M) 256x = 760MiB
			>1M (~1280x720 = 1.4M) 512x = 675MiB
			<1M (~848x480 = 610K) 1024x = 596MiB
		*/
		PipeInfo->BufferSize = buflength > 2097152 ? 0x100 : (buflength > 1048576 ? 0x200 : 0x400);

	if (cmdopts->PipeMT)
	{
		OBuffer.capacity = PipeInfo->BufferSize;
		OBuffer.dynarray = (node_t *) malloc(sizeof(node_t) * OBuffer.capacity);
		memset(OBuffer.dynarray, 0, sizeof(node_t) * OBuffer.capacity);
		OBuffer.head = OBuffer.tail = 0;

		OBuffer.framesize = buflength;
		fprintf( stderr, "avs4x264 [info]: Multi-threaded pipe buffer enabled.\n");
		fprintf( stderr, "avs4x264 [info]: Buffer size set to %dx%d\n", OBuffer.capacity, OBuffer.framesize);
		fflush(stderr);
	}

	//write
	for ( frame = VideoInfo->i_frame_start; frame < VideoInfo->i_frame_total; frame++ )
	{
		if (cmdopts->PipeMT && (frame & 1))
		{
			GetConsoleTitle(title, 1024);
			if (!strchr(title, '$'))
			{
				sprintf(szbuffer, " $ Buffer: %d/%d (%d MB)", OBuffer.size, OBuffer.capacity, (OBuffer.framesize * OBuffer.size) >> 20);
				strcat(title, szbuffer);
				SetConsoleTitle(title);
			}
		}

		frm = avs_h.func.avs_get_frame( avs_h.clip, frame );
		const char *err = avs_h.func.avs_clip_get_error( avs_h.clip );

		if ( err )
		{
			fprintf( stderr, "avs [error]: %s occurred while reading frame %d\n", err, frame );
			return ERR_PROCESS_FAIL;
		}
		planeY = (char *)(frm->vfb->data + frm->offset);
		planeU = (char *)(frm->vfb->data + frm->offsetU);
		planeV = (char *)(frm->vfb->data + frm->offsetV);

		if (cmdopts->PipeMT)
		{
			char *bufyuv = (char *) malloc(buflength);
			char *bufpos = bufyuv;
			for (j = 0; j < VideoInfo->i_height; j++)
			{
				memcpy(bufpos, planeY, VideoInfo->i_width);
				bufpos += VideoInfo->i_width;
				planeY += frm->pitch;
			}
			for (j = 0; j < h_half; j++)
			{
				memcpy(bufpos, planeU, w_half);
				bufpos += w_half;
				planeU += frm->pitchUV;
			}
			for (j = 0; j < h_half; j++)
			{
				memcpy(bufpos, planeV, w_half);
				bufpos += w_half;
				planeV += frm->pitchUV;
			}
			//assert(bufpos - bufyuv == buflength);
			(*Func)(0, bufyuv, buflength);
		}
		else
		{
			for (j = 0; j < VideoInfo->i_height; j++)
			{
				if ( !(*Func)(PipeInfo->h_pipeWrite, planeY, VideoInfo->i_width) )
				{
					fprintf( stderr, "avs [error]: Error occurred while writing frame %d\n(Maybe x264_64.exe closed)\n", frame );
					return ERR_PROCESS_FAIL;
				}
				planeY += frm->pitch;
			}
			for (j = 0; j < VideoInfo->chroma_height; j++)
			{
				if ( !(*Func)(PipeInfo->h_pipeWrite, planeU, VideoInfo->chroma_width) )
				{
					fprintf( stderr, "avs [error]: Error occurred while writing frame %d\n(Maybe x264_64.exe closed)\n", frame );
					return ERR_PROCESS_FAIL;
				}
				planeU += frm->pitchUV;
			}
			for (j = 0; j < VideoInfo->chroma_height; j++)
			{
				if ( !(*Func)(PipeInfo->h_pipeWrite, planeV, VideoInfo->chroma_width) )
				{
					fprintf( stderr, "avs [error]: Error occurred while writing frame %d\n(Maybe x264_64.exe closed)\n", frame );
					return ERR_PROCESS_FAIL;
				}
				planeV += frm->pitchUV;
			}
		}
		avs_h.func.avs_release_video_frame( frm );
	}
}

