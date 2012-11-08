
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.

#define VERSION_MAJOR  0
#define VERSION_MINOR  8
#define VERSION_BUGFIX 0

#include "avs4x264mod.h"

/* load the library and functions we require from it */
int avs_load_library( avs_hnd_t *h );
float get_avs_version( avs_hnd_t avs_h );
AVS_Value update_clip( avs_hnd_t avs_h, const AVS_VideoInfo *vi, AVS_Value res, AVS_Value release );
char * x264_generate_command(cmd_t *cmdopt, x264_cmd_t *xcmdopt, video_info_t *vi);
int _writefile(HANDLE hFile, char *lpBuffer, int nNumberOfBytesToWrite);
int _mt_writefile(HANDLE hFile, char *lpBuffer, int nNumberOfBytesToWrite);

void showhelp(char *prog);
extern buffer_t OBuffer;


int main(int argc, char *argv[])
{
	printf("\n"
	       "avs4x264mod - simple Avisynth pipe tool for x264\n"
	       "Version: %d.%d.%d.%d-excalibur, built on %s, %s\n\n", VERSION_MAJOR, VERSION_MINOR, VERSION_BUGFIX, VERSION_GIT, __DATE__, __TIME__);
	if (argc == 1)
	{
		showhelp(argv[0]);
		return -1;
	}
	int result = 0;
	char *cmd;
	
	cmd_t *cmd_options = (cmd_t *)malloc(sizeof(cmd_t));
	ZeroMemory(cmd_options, sizeof(cmd_t));

	x264_cmd_t *x264cmd_options = (x264_cmd_t *)malloc(sizeof(x264_cmd_t));
	ZeroMemory(x264cmd_options, sizeof(x264_cmd_t));

	parse_opt(&argc, argv, cmd_options);
	if (cmd_options->InFile == 0)
	{
		color_printf( "avs4x264 [error]: No input file found.\n");
		return -1;
	}
	if (cmd_options->InFileType == 0)
	{
		color_printf( "avs4x264 [error]: Unsupported input file found: %s\n", cmd_options->InFile);
		return -1;
	}
	if (cmd_options->Interlaced)
		color_printf( "avs4x264 [info]: Interlaced mode.\n");

	video_info_t *vi = (video_info_t *) malloc(sizeof(video_info_t));
	ZeroMemory(vi, sizeof(video_info_t));
	vi->infile = cmd_options->InFile;
	
	pipe_info_t *pi = (pipe_info_t *) malloc(sizeof(pipe_info_t));
	ZeroMemory(pi, sizeof(pipe_info_t));

	if (cmd_options->Seek > 0)
	{
		vi->i_frame_start = cmd_options->Seek;
		if (cmd_options->TCFile || cmd_options->QPFile || cmd_options->SeekSafe)
		{
			char str[1024];
			sprintf(str, " --seek %d ", cmd_options->Seek);
			strcat(x264cmd_options->extra, str);
		}
	}

	// change affinity before open avs file: much faster on multicore cpu
	if(cmd_options->Affinity)
	{
		color_printf( "avs4x264 [info]: My CPU affinity set to %d\n", cmd_options->Affinity);
		SetProcessAffinityMask(GetCurrentProcess(), cmd_options->Affinity);
	}

	// avs open
	result = LoadAVSFile(vi, cmd_options);
	if (result == ERR_AVS_NOTFOUND)
		return -1;
	if (result == ERR_AVS_FAIL)
		goto avs_fail;
	
	if (cmd_options->Frames)
		vi->i_frame_total = cmd_options->Frames + vi->i_frame_start;

	if ( vi->num_frames < vi->i_frame_total )
	{
		color_printf( "avs4x264 [warning]: %d frame(s) requested, but %d frame(s) given\n", vi->i_frame_total, vi->num_frames );
		vi->i_frame_total = vi->num_frames;
	}

	vi->i_encode_frames = vi->i_frame_total - vi->i_frame_start;

	if ( cmd_options->TCFile || cmd_options->QPFile || cmd_options->SeekSafe )
		/* don't skip the number --seek defines if timecodes/qpfile presents or seek-mode=safe */
		vi->i_frame_start = 0;
	else if ( vi->i_frame_start != 0 )
		color_printf( "avs4x264 [info]: Convert \"--seek %d\" to internal frame skipping\n", vi->i_frame_start );

	x264cmd_options->argc = argc;
	x264cmd_options->argv = argv;
	
	cmd = x264_generate_command(cmd_options, x264cmd_options, vi);
	color_printf( "avs4x264 [info]: %s\n", cmd);

	result = CreateX264Process(cmd, cmd_options, vi, pi);
	if(result == ERR_AVS_FAIL)
		goto avs_fail;
	if(result == ERR_PIPE_FAIL)
		goto pipe_fail;
	free(cmd);

	result = WritePipeLoop(cmd_options->PipeMT ? &_mt_writefile : &_writefile, cmd_options, vi, pi);
	if(result == ERR_PROCESS_FAIL)
		goto process_fail;
	OBuffer.size++;
	
	if(cmd_options->PipeMT)
	{
		// more cleanups
		if(OBuffer.size > 0)
			color_printf( "avs4x264 [info]: Waiting background worker to finish his job...\n" );
		
		while(OBuffer.size > 0)
		{
			color_printf( "\t\t\t\t\t\t\t\t\t\t Buffer: %d <-- \r", OBuffer.size);
			fflush(stderr);
			Sleep(1000);
		}
		OBuffer.size = -1; // and sub-thread will exit automatically.
	}
	
	//avs related
	DWORD exitcode = 0;
	

	//close & cleanup
process_fail:// everything created
	CloseHandle(pi->h_pipeWrite);// h_pipeRead already closed
	WaitForSingleObject(pi->pi_info.hProcess, INFINITE);
	GetExitCodeProcess(pi->pi_info.hProcess, &exitcode);
	CloseHandle(pi->pi_info.hProcess);
	goto avs_cleanup;// pipes already closed
pipe_fail://pipe created but failed after that
	CloseHandle(pi->h_pipeRead);
	CloseHandle(pi->h_pipeWrite);
avs_fail://avs enviormnet created but failed after that
	exitcode = -1;
avs_cleanup:
	avs_cleanup();

	return exitcode;
}

void showhelp(char *prog)
{
	printf("Usage: avs4x264mod [avs4x264mod options] [x264 options] <input>\n\n"        , prog);
	puts("Supported input formats:\n"
		"     .avs\n"
		"     .d2v: requires DGDecode.dll\n"
		"     .dga: requires DGAVCDecode.dll\n"
		"     .dgi: requires DGAVCDecodeDI.dll or DGDecodeNV.dll according to dgi file\n"
	);
	printf("Options:\n");
	printf(" -L, --x264-binary <file>   User defined x264 binary path. [Default=\"%s\"]\n", DEFAULT_BINARY_PATH);
	puts("     --seek-mode <string>   Set seek mode when using --seek. [Default=\"fast\"]\n"
		"                                - fast: Skip process of frames before seek number as x264 does if no\n"
		"                                        --tcfile-in/--qpfile specified;\n"
		"                                        otherwise freeze frames before seek number to skip process, \n"
		"                                        but keep frame number as-is.\n"
		"                                        ( x264 treats tcfile-in/qpfile as timecodes/qpfile of input \n"
		"                                        video, not output video )\n"
		"                                        Normally safe enough for randomly seekable AviSynth scripts.\n"
		"                                        May break scripts which can only be linearly seeked, such as\n"
		"                                        TDecimate(mode=3)\n"
		"                                - safe: Process and deliver every frame to x264.\n"
		"                                        Should give accurate result with every AviSynth script.\n"
		"                                        Significantly slower when the process is heavy.\n");

	puts(" --x264-affinity <cpu>      Set Process Affinity for x264\n"
			" --affinity <cpu>           Set Process Affinity for the piper\n"
			" --pipe-mt                  Seperated thread for pipe buffer\n"
			" --pipe-buffer              Pipe buffer frames [Default: 512, 1024, 2048 depends]\n"
			"\n"
			"   Affinity is in decimal (eg. 63 = 1+2+4+8+16+32 means core 0~5)\n"
			"\n"
			"This program is free software; you can redistribute it and/or modify\n"
			"it under the terms of the GNU General Public License as published by\n"
			"the Free Software Foundation; either version 2 of the License, or\n"
			"(at your option) any later version.");
}
