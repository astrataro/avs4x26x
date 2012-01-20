#ifndef __AVS4X264MOD_H__
#define __AVS4X264MOD_H__

#include <stdio.h>
#include <stdlib.h>
#include <windows.h>
#include <string.h>
#include <ctype.h>

/* the AVS interface currently uses __declspec to link function declarations to their definitions in the dll.
   this has a side effect of preventing program execution if the avisynth dll is not found,
   so define __declspec(dllimport) to nothing and work around this */
#undef __declspec
#define __declspec(i)
#undef EXTERN_C

#include "avisynth_c.h"
#include "version.h"

#define PIPE_BUFFER_SIZE (DWORD)0//1048576 // values bigger than 250000 break the application

/* AVS uses a versioned interface to control backwards compatibility */
/* YV12 support is required */
#define AVS_INTERFACE_YV12 2
/* when AVS supports other planar colorspaces, a workaround is required */
#define AVS_INTERFACE_OTHER_PLANAR 5

/* maximum size of the sequence of filters to try on non script files */
#define AVS_MAX_SEQUENCE 5

#define LOAD_AVS_FUNC(name, continue_on_fail) \
{\
	h->func.name = (void*)GetProcAddress( h->library, #name );\
	if( !continue_on_fail && !h->func.name )\
		goto fail;\
}

typedef struct
{
	AVS_Clip *clip;
	AVS_ScriptEnvironment *env;
	HMODULE library;
	/* declare function pointers for the utilized functions to be loaded without __declspec,
	   as the avisynth header does not compensate for this type of usage */
	struct
	{
		const char *(__stdcall *avs_clip_get_error)( AVS_Clip *clip );
		AVS_ScriptEnvironment *(__stdcall *avs_create_script_environment)( int version );
		void (__stdcall *avs_delete_script_environment)( AVS_ScriptEnvironment *env );
		AVS_VideoFrame *(__stdcall *avs_get_frame)( AVS_Clip *clip, int n );
		int (__stdcall *avs_get_version)( AVS_Clip *clip );
		const AVS_VideoInfo *(__stdcall *avs_get_video_info)( AVS_Clip *clip );
		int (__stdcall *avs_function_exists)( AVS_ScriptEnvironment *env, const char *name );
		AVS_Value (__stdcall *avs_invoke)( AVS_ScriptEnvironment *env, const char *name,
			AVS_Value args, const char **arg_names );
		void (__stdcall *avs_release_clip)( AVS_Clip *clip );
		void (__stdcall *avs_release_value)( AVS_Value value );
		void (__stdcall *avs_release_video_frame)( AVS_VideoFrame *frame );
		AVS_Clip *(__stdcall *avs_take_clip)( AVS_Value, AVS_ScriptEnvironment *env );
	} func;
} avs_hnd_t;

typedef struct
{
	char *infile;
	int i_width;
	int i_height;
	int i_fps_num;
	int i_fps_den;
	int i_frame_start;
	int i_frame_total;
	int i_encode_frames;
	int num_frames;
	char* csp;
	unsigned int chroma_height, chroma_width;
} video_info_t;

typedef struct
{
	HANDLE h_process, h_stdOut, h_pipeRead, h_pipeWrite, h_stdErr;
	SECURITY_ATTRIBUTES saAttr;
	STARTUPINFO si_info;
	PROCESS_INFORMATION pi_info;
	int BufferSize;
	char VFR;
} pipe_info_t;

typedef struct
{
	char * data;
	int length;
} node_t;

typedef struct
{
	int size;
	int capacity;
	int head;
	int tail;
	node_t* dynarray;
	int exitcode;
	int framesize;
} buffer_t;

typedef struct
{
	int Affinity;
	int x264Affinity;
	int BufferSize;
	int SeekSafe;
	int PipeMT;
	int Interlaced;
	int QPFile;
	int TCFile;
	int Seek;
	int Frames;
	char* X264Path;
	char* InFile;
} cmd_t;

typedef struct
{
	char** argv;
	int argc;
	char extra[8192];
} x264_cmd_t;

#define ERR_AVS_NOTFOUND -1
#define ERR_AVS_FAIL -2
#define ERR_PIPE_FAIL -3
#define ERR_PROCESS_FAIL -4

#define DEFAULT_BINARY_PATH "x264_64"

#endif