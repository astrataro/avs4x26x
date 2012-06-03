
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.

#define VERSION_MAJOR  0
#define VERSION_MINOR  7
#define VERSION_BUGFIX 0

#include <stdio.h>
#include <stdlib.h>
#include <windows.h>
#include <string.h>
#include <ctype.h>
#include <math.h>

/* the AVS interface currently uses __declspec to link function declarations to their definitions in the dll.
   this has a side effect of preventing program execution if the avisynth dll is not found,
   so define __declspec(dllimport) to nothing and work around this */
#undef __declspec
#define __declspec(i)
#undef EXTERN_C

#include "avisynth_c.h"
#include "version.h"

#define DEFAULT_BINARY_PATH "x264_64"

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

/* load the library and functions we require from it */
static int avs_load_library( avs_hnd_t *h )
{
    h->library = LoadLibrary( "avisynth" );
    if( !h->library )
        return -1;
    LOAD_AVS_FUNC( avs_clip_get_error, 0 );
    LOAD_AVS_FUNC( avs_create_script_environment, 0 );
    LOAD_AVS_FUNC( avs_delete_script_environment, 1 );
    LOAD_AVS_FUNC( avs_get_frame, 0 );
    LOAD_AVS_FUNC( avs_get_version, 0 );
    LOAD_AVS_FUNC( avs_get_video_info, 0 );
    LOAD_AVS_FUNC( avs_function_exists, 0 );
    LOAD_AVS_FUNC( avs_invoke, 0 );
    LOAD_AVS_FUNC( avs_release_clip, 0 );
    LOAD_AVS_FUNC( avs_release_value, 0 );
    LOAD_AVS_FUNC( avs_release_video_frame, 0 );
    LOAD_AVS_FUNC( avs_take_clip, 0 );
    return 0;
fail:
    FreeLibrary( h->library );
    return -1;
}

/*
 * @deprecated Use get_avs_version_string instead
 *
static float get_avs_version_number( avs_hnd_t avs_h )
{
    if( !avs_h.func.avs_function_exists( avs_h.env, "VersionNumber" ) )
    {
       fprintf( stderr, "avs [error]: VersionNumber does not exist\n" );
       return -1;
    }
    AVS_Value ver = avs_h.func.avs_invoke( avs_h.env, "VersionNumber", avs_new_value_array( NULL, 0 ), NULL );
    if( avs_is_error( ver ) )
    {
       fprintf( stderr, "avs [error]: Unable to determine avisynth version: %s\n", avs_as_error( ver ) );
       return -1;
    }
    if( !avs_is_float( ver ) )
    {
       fprintf( stderr, "avs [error]: VersionNumber did not return a float value\n" );
       return -1;
    }
    float ret = avs_as_float( ver );
    avs_h.func.avs_release_value( ver );
    return ret;
}
 *
 */

static char const *get_avs_version_string( avs_hnd_t avs_h )
{
    if( !avs_h.func.avs_function_exists( avs_h.env, "VersionString" ) )
    {
       fprintf( stderr, "avs [error]: VersionString does not exist\n" );
       return "AviSynth: unknown version";
    }
    AVS_Value ver = avs_h.func.avs_invoke( avs_h.env, "VersionString", avs_new_value_array( NULL, 0 ), NULL );
    if( avs_is_error( ver ) )
    {
       fprintf( stderr, "avs [error]: Unable to determine avisynth version: %s\n", avs_as_error( ver ) );
       return "AviSynth: unknown version";
    }
    if( !avs_is_string( ver ) )
    {
       fprintf( stderr, "avs [error]: VersionString did not return a string value\n" );
       return "AviSynth: unknown version";
    }
    const char *ret = avs_as_string( ver );
    avs_h.func.avs_release_value( ver );
    return ret;
}

static AVS_Value update_clip( avs_hnd_t avs_h, const AVS_VideoInfo *vi, AVS_Value res, AVS_Value release )
{
    avs_h.func.avs_release_clip( avs_h.clip );
    avs_h.clip = avs_h.func.avs_take_clip( res, avs_h.env );
    avs_h.func.avs_release_value( release );
    vi = avs_h.func.avs_get_video_info( avs_h.clip );
    return res;
}

char* generate_new_commadline(int argc, char *argv[], int i_frame_total, int i_fps_num, int i_fps_den, int i_width, int i_height, char* infile, const char* csp, int b_tc, int i_encode_frames )
{
    int i;
    char *cmd, *buf;
    int b_add_fps      = 1;
    int b_add_csp      = 1;
    int b_add_res      = 1;
    int b_add_timebase = b_tc;
    char *x264_binary;
    x264_binary = DEFAULT_BINARY_PATH;
    buf = malloc(20);
    *buf=0;
    cmd = malloc(8192);
    for (i=1;i<argc;i++)
    {
        if( !strncmp(argv[i], "--x264-binary", 13) || !strncmp(argv[i], "-L", 2) )
        {
            if( !strcmp(argv[i], "--x264-binary") || !strcmp(argv[i], "-L") )
            {
                x264_binary = argv[i+1];
                for (int k=i;k<argc-2;k++)
                    argv[k] = argv[k+2];
                argc -= 2;
            }
            else if( !strncmp(argv[i], "--x264-binary=", 14) )
            {
                x264_binary = argv[i]+14;
                for (int k=i;k<argc-1;k++)
                    argv[k] = argv[k+1];
                argc--;
            }
            else if( !strncmp(argv[i], "-L=", 3) )
            {
                x264_binary = argv[i]+3;
                for (int k=i;k<argc-1;k++)
                    argv[k] = argv[k+1];
                argc--;
            }
            else                           /* else argv[i] should have structure like -Lx264 */
            {
                x264_binary = argv[i]+2;
                for (int k=i;k<argc-1;k++)
                    argv[k] = argv[k+1];
                argc--;
            }
            i--;
        }
    }
    if ( b_tc )
    {
        b_add_fps = 0;
        for (i=1;i<argc;i++)
        {
            if( !strncmp(argv[i], "--timebase", 10) )
            {
                b_add_timebase = 0;
                break;
            }
        }
    }
    else
    {
        for (i=1;i<argc;i++)
        {
            if( !strncmp(argv[i], "--fps", 5) )
            {
                b_add_fps = 0;
                break;
            }
        }
    }
    for (i=1;i<argc;i++)
    {
        if( !strncmp(argv[i], "--input-csp", 11) )
        {
            b_add_csp = 0;
            break;
        }
    }
    for (i=1;i<argc;i++)
    {
        if( !strncmp(argv[i], "--input-res", 11) )
        {
            b_add_res = 0;
            break;
        }
    }
    for (i=argc-1;i>0;i--)
    {
        if( !strncmp(argv[i], "--input-depth", 13) )
        {
            if( !strcmp(argv[i], "--input-depth") )
            {
                if( strcmp(argv[++i], "8") )
                {
                    i_width >>= 1;
                    fprintf( stdout, "avs4x264 [info]: High bit depth detected, resolution corrected\n" );
                }
            }
            else if( strcmp(argv[i], "--input-depth=8") )
            {
                i_width >>= 1;
                fprintf( stdout, "avs4x264 [info]: High bit depth detected, resolution corrected\n" );
            }
            break;
        }
    }

    sprintf(cmd, "\"%s\" - ", x264_binary);

    for (i=1;i<argc;i++)
    {
        if ( infile!=argv[i] || !strcmp(argv[i-1], "--audiofile") )
        {
            if (strrchr(argv[i], ' '))
            {
                strcat(cmd, "\"");
                strcat(cmd, argv[i]);
                strcat(cmd, "\"");
            }
            else
                strcat(cmd, argv[i]);
            if(i<argc-1)
                strcat(cmd, " ");
        }
    }

    sprintf(buf, " --frames %d", i_encode_frames);
    strcat(cmd, buf);

    if ( b_add_fps )
    {
        sprintf(buf, " --fps %d/%d", i_fps_num, i_fps_den);
        strcat(cmd, buf);
    }
    if ( b_add_timebase )
    {
        sprintf(buf, " --timebase %d", i_fps_den);
        strcat(cmd, buf);
    }
    if ( b_add_res )
    {
        sprintf(buf, " --input-res %dx%d", i_width, i_height);
        strcat(cmd, buf);
    }
    if ( b_add_csp )
    {
        sprintf(buf, " --input-csp %s", csp);
        strcat(cmd, buf);
    }
    free(buf);
    return cmd;
}

int main(int argc, char *argv[])
{
    //avs related
    avs_hnd_t avs_h;
    AVS_Value arg;
    AVS_Value res;
    char *filter = NULL;
    // float avs_version_number;
    const char *avs_version_string;
    AVS_VideoFrame *frm;
    //createprocess related
    HANDLE h_process, h_stdOut, h_stdErr, h_pipeRead, h_pipeWrite;
    SECURITY_ATTRIBUTES saAttr;
    STARTUPINFO si_info;
    PROCESS_INFORMATION pi_info;
    DWORD exitcode = 0;
    /*Video Info*/
    int i_width;
    int i_height;
    int i_fps_num;
    int i_fps_den;
    int i_frame_start=0;
    int i_frame_total;
    int b_interlaced=0;
    int b_qp=0;
    int b_tc=0;
    int b_seek_safe=0;
    int b_change_frame_total=0;
    int i_encode_frames;
    /*Video Info End*/
    char *planeY, *planeU, *planeV;
    unsigned int frame,len,chroma_height,chroma_width;
    int i,j;
    char *cmd;
    char *infile = NULL;
    const char *csp = NULL;
    if (argc>1)
    {
        //get the script file and other informations from the commandline

        for (i=1;i<argc;i++)
        {
            if( !strcmp(argv[i], "--interlaced") || !strcmp(argv[i], "--tff") || !strcmp(argv[i], "--bff") )
            {
                fprintf( stderr, "%s found.\n", argv[i]);
                b_interlaced=1;
                break;
            }
        }
        for (i=1;i<argc;i++)
        {
            if( !strncmp(argv[i], "--qpfile", 8) )
            {
                b_qp = 1;
                break;
            }
        }
        for (i=1;i<argc;i++)
        {
            if( !strncmp(argv[i], "--tcfile-in", 11) )
            {
                b_tc = 1;
                break;
            }
        }
        for (i=1;i<argc;i++)
        {
            if( !strncmp(argv[i], "--seek-mode", 11) )
            {
                if( !strcmp(argv[i], "--seek-mode") )
                {
                    if( !strcasecmp(argv[i+1], "safe" ) )
                    {
                        b_seek_safe = 1;
                    }
                    else if( !strcasecmp(argv[i+1], "fast" ) )
                    {
                        b_seek_safe = 0;
                    }
                    else
                    {
                        fprintf( stderr, "avs4x264 [error]: invalid seek-mode\n" );
                        return -1;
                    }
                    for (int k=i;k<argc-2;k++)
                        argv[k] = argv[k+2];
                    argc -= 2;
                }
                else if( !strncmp(argv[i], "--seek-mode=", 12) )
                {
                    if( !strcasecmp(argv[i]+12, "safe" ) )
                    {
                        b_seek_safe = 1;
                    }
                    else if( !strcasecmp(argv[i]+12, "fast" ) )
                    {
                        b_seek_safe = 0;
                    }
                    else
                    {
                        fprintf( stderr, "avs4x264 [error]: invalid seek-mode\n" );
                        return -1;
                    }
                    for (int k=i;k<argc-1;k++)
                        argv[k] = argv[k+1];
                    argc--;
                }
                i--;
            }
        }

        //avs open
        if( avs_load_library( &avs_h ) )
        {
           fprintf( stderr, "avs [error]: failed to load avisynth\n" );
           return -1;
        }
        avs_h.env = avs_h.func.avs_create_script_environment( AVS_INTERFACE_YV12 );
        if( !avs_h.env )
        {
           fprintf( stderr, "avs [error]: failed to initiate avisynth\n" );
           goto avs_fail;
        }

        for (i=1;i<argc;i++)
        {
            len =  strlen(argv[i]);

            if ( !strncmp(argv[i], "--audiofile=", 12) || !strncmp(argv[i], "--output=", 9) )
                continue;
            else if ( !strncmp(argv[i], "-o", 2) && strcmp(argv[i], "-o") )                   // special case: -ofilename.ext equals to --output filename.ext
                continue;
            else if ( !strcmp(argv[i], "--output") || !strcmp(argv[i], "-o") || !strcmp(argv[i], "--audiofile") )
            {
                i++;
                continue;
            }
            else if ( len>4 &&
                 (argv[i][len-4])== '.' &&
                 tolower(argv[i][len-3])== 'a' &&
                 tolower(argv[i][len-2])== 'v' &&
                 tolower(argv[i][len-1])== 's' )
            {
                infile=argv[i];
                arg = avs_new_value_string( infile );
                res = avs_h.func.avs_invoke( avs_h.env, "Import", arg, NULL );
                if( avs_is_error( res ) )
                {
                    fprintf( stderr, "avs [error]: %s\n", avs_as_string( res ) );
                    goto avs_fail;
                }
                break;
            }

            else if ( len>4 &&
                      (argv[i][len-4])== '.' &&
                      tolower(argv[i][len-3])== 'd' &&
                      tolower(argv[i][len-2])== '2' &&
                      tolower(argv[i][len-1])== 'v' )
            {
                infile=argv[i];
                filter = "MPEG2Source";
                if( !avs_h.func.avs_function_exists( avs_h.env, filter ) )
                {
                    fprintf( stderr, "avs4x264 [error]: %s not found\n", filter );
                    goto avs_fail;
                }
                arg = avs_new_value_string( infile );
                res = avs_h.func.avs_invoke( avs_h.env, filter, arg, NULL );
                if( avs_is_error( res ) )
                {
                    fprintf( stderr, "avs [error]: %s\n", avs_as_string( res ) );
                    goto avs_fail;
                }
                break;
            }

            else if ( len>4 &&
                      (argv[i][len-4])== '.' &&
                      tolower(argv[i][len-3])== 'd' &&
                      tolower(argv[i][len-2])== 'g' &&
                      tolower(argv[i][len-1])== 'a' )
            {
                infile=argv[i];
                filter = "AVCSource";
                if( !avs_h.func.avs_function_exists( avs_h.env, filter ) )
                {
                    fprintf( stderr, "avs4x264 [error]: %s not found\n", filter );
                    goto avs_fail;
                }
                arg = avs_new_value_string( infile );
                res = avs_h.func.avs_invoke( avs_h.env, filter, arg, NULL );
                if( avs_is_error( res ) )
                {
                    fprintf( stderr, "avs [error]: %s\n", avs_as_string( res ) );
                    goto avs_fail;
                }
                break;
            }

            else if ( len>4 &&
                      (argv[i][len-4])== '.' &&
                      tolower(argv[i][len-3])== 'd' &&
                      tolower(argv[i][len-2])== 'g' &&
                      tolower(argv[i][len-1])== 'i' )
            {
                infile=argv[i];
                filter = "DGSource";
                if( !avs_h.func.avs_function_exists( avs_h.env, filter ) )
                {
                    fprintf( stderr, "avs4x264 [error]: %s not found\n", filter );
                    goto avs_fail;
                }
                arg = avs_new_value_string( infile );
                res = avs_h.func.avs_invoke( avs_h.env, filter, arg, NULL );
                if( avs_is_error( res ) )
                {
                    fprintf( stderr, "avs [error]: %s\n", avs_as_string( res ) );
                    goto avs_fail;
                }
                break;
            }

            else if ( len>4 &&
                      (argv[i][len-4])== '.' &&
                      tolower(argv[i][len-3])== 'a' &&
                      tolower(argv[i][len-2])== 'v' &&
                      tolower(argv[i][len-1])== 'i' )
            {
                infile=argv[i];
                filter = "AVISource";
                arg = avs_new_value_string( infile );
                res = avs_h.func.avs_invoke( avs_h.env, filter, arg, NULL );
                if( avs_is_error( res ) )
                {
                    fprintf( stderr, "avs [error]: %s\n", avs_as_string( res ) );
                    goto source_ffms_general;
                }
                else
                    break;
            }

            else if ( ( len>5 && (argv[i][len-5])== '.' && (   ( tolower(argv[i][len-4])== 'm' && argv[i][len-3]== '2' && tolower(argv[i][len-2])== 't' && tolower(argv[i][len-1])== 's' )  // m2ts
                                                            || ( tolower(argv[i][len-4])== 'm' && argv[i][len-3]== 'p' && tolower(argv[i][len-2])== 'e' && tolower(argv[i][len-1])== 'g' )  // mpeg
                                                           )
                      )
                   || ( len>4 && (argv[i][len-4])== '.' && (   ( tolower(argv[i][len-3])== 'v' && tolower(argv[i][len-2])== 'o' && tolower(argv[i][len-1])== 'b' )                         // vob
                                                            || ( tolower(argv[i][len-3])== 'm' && tolower(argv[i][len-2])== '2' && tolower(argv[i][len-1])== 'v' )                         // m2v
                                                            || ( tolower(argv[i][len-3])== 'm' && tolower(argv[i][len-2])== 'p' && tolower(argv[i][len-1])== 'g' )                         // mpg
                                                            || ( tolower(argv[i][len-3])== 'o' && tolower(argv[i][len-2])== 'g' && tolower(argv[i][len-1])== 'v' )                         // ogv
                                                            || ( tolower(argv[i][len-3])== 'o' && tolower(argv[i][len-2])== 'g' && tolower(argv[i][len-1])== 'm' )                         // ogm
                                                           )
                      )
                   || ( len>3 && (argv[i][len-3])== '.' && (   ( tolower(argv[i][len-2])== 't' && tolower(argv[i][len-1])== 's' )                                                           // ts
                                                            || ( tolower(argv[i][len-2])== 't' && tolower(argv[i][len-1])== 'p' )                                                           // tp
                                                            || ( tolower(argv[i][len-2])== 'p' && tolower(argv[i][len-1])== 's' )                                                           // ps
                                                           )
                      )
                    )                                               /* We don't trust ffms's non-linear seeking for these formats */
            {
                infile=argv[i];
                filter = "FFIndex";
                if( avs_h.func.avs_function_exists( avs_h.env, filter ) )
                {
                    AVS_Value arg_arr[] = { avs_new_value_string( infile ), avs_new_value_string( "lavf" ) };
                    const char *arg_name[] = { "source", "demuxer" };
                    res = avs_h.func.avs_invoke( avs_h.env, filter, avs_new_value_array( arg_arr, 2 ), arg_name );
                    if( avs_is_error( res ) )
                    {
                        fprintf( stderr, "avs [error]: %s\n", avs_as_string( res ) );
                        goto source_dss;
                    }
                }
                else
                {
                    fprintf( stderr, "avs4x264 [error]: %s not found\n", filter );
                    goto source_dss;
                }

                filter = "FFVideoSource";
                if( avs_h.func.avs_function_exists( avs_h.env, filter ) )
                {
                    AVS_Value arg_arr[] = { avs_new_value_string( infile ), avs_new_value_int( 1 ), avs_new_value_int( -1 ) };
                    const char *arg_name[] = { "source", "threads", "seekmode" };
                    res = avs_h.func.avs_invoke( avs_h.env, filter, avs_new_value_array( arg_arr, 3 ), arg_name );
                    if( avs_is_error( res ) )
                    {
                        fprintf( stderr, "avs [error]: %s\n", avs_as_string( res ) );
                        goto source_dss;
                    }
                    else
                    {
                        fprintf( stdout, "avs4x264 [info]: No safe non-linear seeking guaranteed for input file, force seek-mode=safe\n" );
                        b_seek_safe = 1;
                    }
                }
                else
                {
                    fprintf( stderr, "avs4x264 [error]: %s not found\n", filter );
                    goto source_dss;
                }
                break;
            }

            else if ( ( len>4 && (argv[i][len-4])== '.' && (   ( tolower(argv[i][len-3])== 'm' && tolower(argv[i][len-2])== 'k' && tolower(argv[i][len-1])== 'v' )  // mkv
                                                            || ( tolower(argv[i][len-3])== 'm' && tolower(argv[i][len-2])== 'p' && tolower(argv[i][len-1])== '4' )  // mp4
                                                            || ( tolower(argv[i][len-3])== 'm' && tolower(argv[i][len-2])== '4' && tolower(argv[i][len-1])== 'v' )  // m4v
                                                            || ( tolower(argv[i][len-3])== 'm' && tolower(argv[i][len-2])== 'o' && tolower(argv[i][len-1])== 'v' )  // mov
                                                            || ( tolower(argv[i][len-3])== 'f' && tolower(argv[i][len-2])== 'l' && tolower(argv[i][len-1])== 'v' )  // flv
                                                           )
                      )
                   || ( len>5 && (argv[i][len-5])== '.' && tolower(argv[i][len-4])== 'w' && tolower(argv[i][len-3])== 'e' && tolower(argv[i][len-2])== 'b' && tolower(argv[i][len-1])== 'm' )  // webm
                    )                                              /* Non-linear seeking seems to be reliable for these formats */
            {
                infile=argv[i];
source_ffms_general:
                filter = "FFVideoSource";
                if( avs_h.func.avs_function_exists( avs_h.env, filter ) )
                {
                    AVS_Value arg_arr[] = { avs_new_value_string( infile ), avs_new_value_int( 1 ) };
                    const char *arg_name[] = { "source", "threads" };
                    res = avs_h.func.avs_invoke( avs_h.env, filter, avs_new_value_array( arg_arr, 2 ), arg_name );
                    if( avs_is_error( res ) )
                    {
                        fprintf( stderr, "avs [error]: %s\n", avs_as_string( res ) );
                        goto source_dss;
                    }
                }
                else
                {
                    fprintf( stderr, "avs4x264 [error]: %s not found\n", filter );
                    goto source_dss;
                }
                break;
            }

            else if ( ( len>5 && (argv[i][len-5])== '.' && (   ( tolower(argv[i][len-4])== 'r' && argv[i][len-3]== 'm' && tolower(argv[i][len-2])== 'v' && tolower(argv[i][len-1])== 'b' )  // rmvb
                                                            || ( tolower(argv[i][len-4])== 'd' && argv[i][len-3]== 'i' && tolower(argv[i][len-2])== 'v' && tolower(argv[i][len-1])== 'x' )  // divx
                                                           )
                      )
                   || ( len>4 && (argv[i][len-4])== '.' && (   ( tolower(argv[i][len-3])== 'w' && tolower(argv[i][len-2])== 'm' && tolower(argv[i][len-1])== 'v' )                         // wmv
                                                            || ( tolower(argv[i][len-3])== 'w' && tolower(argv[i][len-2])== 'm' && tolower(argv[i][len-1])== 'p' )                         // wmp
                                                            || ( tolower(argv[i][len-3])== 'a' && tolower(argv[i][len-2])== 's' && tolower(argv[i][len-1])== 'f' )                         // asf
                                                           )
                      )
                   || ( len>3 && (argv[i][len-3])== '.' && (   ( tolower(argv[i][len-2])== 'r' && tolower(argv[i][len-1])== 'm' )                                                           // rm
                                                            || ( tolower(argv[i][len-2])== 'w' && tolower(argv[i][len-1])== 'm' )                                                           // wm
                                                           )
                      )
                    )                                              /* Only use DSS2/DirectShowSource for these formats */
            {
                infile=argv[i];
source_dss:
                filter = "DSS2";
                if( avs_h.func.avs_function_exists( avs_h.env, filter ) )
                {
                    arg = avs_new_value_string( infile );
                    res = avs_h.func.avs_invoke( avs_h.env, filter, arg, NULL );
                    if( avs_is_error( res ) )
                    {
                        fprintf( stderr, "avs [error]: %s\n", avs_as_string( res ) );
                    }
                    else
                        break;
                }
                else
                    fprintf( stderr, "avs4x264 [error]: %s not found\n", filter );

                filter = "DirectShowSource";
                if( avs_h.func.avs_function_exists( avs_h.env, filter ) )
                {
                    AVS_Value arg_arr[] = { avs_new_value_string( infile ), avs_new_value_bool( 0 ) };
                    const char *arg_name[] = { NULL, "audio" };
                    res = avs_h.func.avs_invoke( avs_h.env, filter, avs_new_value_array( arg_arr, 2 ), arg_name );
                    if( avs_is_error( res ) )
                    {
                        fprintf( stderr, "avs [error]: %s\n", avs_as_string( res ) );
                    }
                    else
                        break;
                }
                else
                {
                    fprintf( stderr, "avs4x264 [error]: %s not found\n", filter );
                }
                goto avs_fail;
                break;
            }

        }

        if (!infile)
        {
            fprintf( stderr, "avs4x264 [error]: No supported input file found.\n");
            goto avs_fail;
        }

        if (filter)
            fprintf( stdout, "avs4x264 [info]: using \"%s\" as source filter\n", filter );

        /* check if the user is using a multi-threaded script and apply distributor if necessary.
           adapted from avisynth's vfw interface */
        AVS_Value mt_test = avs_h.func.avs_invoke( avs_h.env, "GetMTMode", avs_new_value_bool( 0 ), NULL );
        int mt_mode = avs_is_int( mt_test ) ? avs_as_int( mt_test ) : 0;
        avs_h.func.avs_release_value( mt_test );
        if( mt_mode > 0 && mt_mode < 5 )
        {
            AVS_Value temp = avs_h.func.avs_invoke( avs_h.env, "Distributor", res, NULL );
            avs_h.func.avs_release_value( res );
            res = temp;
        }

        if( !avs_is_clip( res ) )
        {
            fprintf( stderr, "avs [error]: `%s' didn't return a video clip\n", infile );
            goto avs_fail;
        }
        avs_h.clip = avs_h.func.avs_take_clip( res, avs_h.env );
        // avs_version_number = get_avs_version_number( avs_h );
        // fprintf( stdout, "avs [info]: AviSynth version: %.2f\n", avs_version_number );
        avs_version_string = get_avs_version_string( avs_h );
        fprintf( stdout, "avs [info]: %s\n", avs_version_string );
        const AVS_VideoInfo *vi = avs_h.func.avs_get_video_info( avs_h.clip );
        if( !avs_has_video( vi ) )
        {
            fprintf( stderr, "avs [error]: `%s' has no video data\n", infile );
            goto avs_fail;
        }
        if( vi->width&1 || vi->height&1 )
        {
            fprintf( stderr, "avs [error]: input clip width or height not divisible by 2 (%dx%d)\n",
                     vi->width, vi->height );
            goto avs_fail;
        }
        if ( avs_is_yv12( vi ) )
        {
            csp = "i420";
            chroma_width = vi->width >> 1;
            chroma_height = vi->height >> 1;
            fprintf( stdout, "avs [info]: Video colorspace: YV12\n" );
        }
        else if ( avs_is_yv24( vi ) )
        {
            csp = "i444";
            chroma_width = vi->width;
            chroma_height = vi->height;
            fprintf( stdout, "avs [info]: Video colorspace: YV24\n" );
        }
        else if ( avs_is_yv16( vi ) )
        {
            csp = "i422";
            chroma_width = vi->width >> 1;
            chroma_height = vi->height;
            fprintf( stdout, "avs [info]: Video colorspace: YV16\n" );
        }
        else
        {
            fprintf( stderr, "avs [warning]: Converting input clip to YV12\n" );
            const char *arg_name[2] = { NULL, "interlaced" };
            AVS_Value arg_arr[2] = { res, avs_new_value_bool( b_interlaced ) };
            AVS_Value res2 = avs_h.func.avs_invoke( avs_h.env, "ConvertToYV12", avs_new_value_array( arg_arr, 2 ), arg_name );
            if( avs_is_error( res2 ) )
            {
                fprintf( stderr, "avs [error]: Couldn't convert input clip to YV12\n" );
                goto avs_fail;
            }
            res = update_clip( avs_h, vi, res2, res );
            csp = "i420";
            chroma_width = vi->width >> 1;
            chroma_height = vi->height >> 1;
        }

        for (i=1;i<argc;i++)
        {
            if( !strncmp(argv[i], "--seek", 6) )
            {
                if( !strcmp(argv[i], "--seek") )
                {
                    i_frame_start = atoi(argv[i+1]);
                    if( !b_tc && !b_qp && !b_seek_safe )   /* delete seek parameters if no timecodes/qpfile and seek-mode=fast */
                    {
                        for (int k=i;k<argc-2;k++)
                            argv[k] = argv[k+2];
                        argc -= 2;
                        i--;
                    }
                }
                else
                {
                    i_frame_start = atoi(argv[i]+7);
                    if( !b_tc && !b_qp && !b_seek_safe )   /* delete seek parameters if no timecodes/qpfile and seek-mode=fast */
                    {
                        for (int k=i;k<argc-1;k++)
                            argv[k] = argv[k+1];
                        argc -= 1;
                        i--;
                    }
                }
            }
        }
        if ( !b_seek_safe && i_frame_start && ( b_qp || b_tc ) )
        {
            fprintf( stdout, "avs4x264 [info]: seek-mode=fast with qpfile or timecodes in, freeze first %d %s for fast processing\n", i_frame_start, i_frame_start==1 ? "frame" : "frames" );
            AVS_Value arg_arr[4] = { res, avs_new_value_int( 0 ), avs_new_value_int( i_frame_start ), avs_new_value_int( i_frame_start ) };
            AVS_Value res2 = avs_h.func.avs_invoke( avs_h.env, "FreezeFrame", avs_new_value_array( arg_arr, 4 ), NULL );
            if( avs_is_error( res2 ) )
            {
                fprintf( stderr, "avs [error]: Couldn't freeze first %d %s\n", i_frame_start, i_frame_start==1 ? "frame" : "frames" );
                goto avs_fail;
            }
            res = update_clip( avs_h, vi, res2, res );
        }

        avs_h.func.avs_release_value( res );

        i_width = vi->width;
        i_height = vi->height;
        i_fps_num = vi->fps_numerator;
        i_fps_den = vi->fps_denominator;
        i_frame_total = vi->num_frames;

        if( i_fps_den != 1 )
        {
            double f_fps = (double)i_fps_num / i_fps_den;
            int i_nearest_NTSC_num = (int)(f_fps * 1.001 + 0.5);
            const double f_epsilon = 0.01;

            if( fabs(f_fps - i_nearest_NTSC_num / 1.001) < f_epsilon )
            {
                i_fps_num = i_nearest_NTSC_num * 1000;
                i_fps_den = 1001;
            }
        }

        fprintf( stdout, "avs [info]: Video resolution: %dx%d\n"
                         "avs [info]: Video framerate: %d/%d\n"
                         "avs [info]: Video framecount: %d\n",
                 i_width, i_height, i_fps_num, i_fps_den, i_frame_total );

        //execute the commandline
        h_process = GetCurrentProcess();
        h_stdOut = GetStdHandle(STD_OUTPUT_HANDLE);
        h_stdErr = GetStdHandle(STD_ERROR_HANDLE);

        if (h_stdOut==INVALID_HANDLE_VALUE || h_stdErr==INVALID_HANDLE_VALUE)
        {
            fprintf( stderr, "Error: Couldn\'t get standard handles!");
            goto avs_fail;
        }

        saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
        saAttr.bInheritHandle = TRUE;
        saAttr.lpSecurityDescriptor = NULL;

        if (!CreatePipe(&h_pipeRead, &h_pipeWrite, &saAttr, PIPE_BUFFER_SIZE))
        {
            fprintf( stderr, "Error: Pipe creation failed!");
            goto avs_fail;
        }

        if ( !SetHandleInformation(h_pipeWrite, HANDLE_FLAG_INHERIT, 0) )
        {
            fprintf( stderr, "Error: SetHandleInformation");
            goto pipe_fail;
        }

        ZeroMemory( &pi_info, sizeof(PROCESS_INFORMATION) );
        ZeroMemory( &si_info, sizeof(STARTUPINFO) );
        si_info.cb = sizeof(STARTUPINFO);
        si_info.dwFlags = STARTF_USESTDHANDLES;
        si_info.hStdInput = h_pipeRead;
        si_info.hStdOutput = h_stdOut;
        si_info.hStdError = h_stdErr;

        for (i=1;i<argc;i++)
        {
            if( !strncmp(argv[i], "--frames", 8) )
            {
                if( !strcmp(argv[i], "--frames") )
                {
                    i_frame_total = atoi(argv[i+1]);
                    for (int k=i;k<argc-2;k++)
                        argv[k] = argv[k+2];
                    argc -= 2;
                }
                else
                {
                    i_frame_total = atoi(argv[i]+9);
                    for (int k=i;k<argc-1;k++)
                        argv[k] = argv[k+1];
                    argc -= 1;
                }
                i--;
                b_change_frame_total = 1;
            }
        }
        if ( b_change_frame_total )
            i_frame_total += i_frame_start; /* ending frame should add offset of i_frame_start, not needed if not set as will be clamped */

        if ( vi->num_frames < i_frame_total )
        {
            fprintf( stderr, "avs4x264 [warning]: x264 is trying to encode until frame %d, but input clip has only %d %s\n",
                     i_frame_total, vi->num_frames, vi->num_frames > 1 ? "frames" : "frame" );
            i_frame_total = vi->num_frames;
        }

        i_encode_frames = i_frame_total - i_frame_start;

        if ( b_tc || b_qp || b_seek_safe )      /* don't skip the number --seek defines if has timecodes/qpfile or seek-mode=safe */
        {
            i_frame_start = 0;
        }
        else if ( i_frame_start != 0 )
        {
            fprintf( stdout, "avs4x264 [info]: Convert \"--seek %d\" to internal frame skipping\n", i_frame_start );
        }

        cmd = generate_new_commadline(argc, argv, i_frame_total, i_fps_num, i_fps_den, i_width, i_height, infile, csp, b_tc, i_encode_frames );
        fprintf( stdout, "avs4x264 [info]: %s\n", cmd);

        if (!CreateProcess(NULL, cmd, NULL, NULL, TRUE, 0, NULL, NULL, &si_info, &pi_info))
        {
            fprintf( stderr, "Error: Failed to create process <%d>!", (int)GetLastError());
            free(cmd);
            goto pipe_fail;
        }
        //cleanup before writing to pipe
        CloseHandle(h_pipeRead);
        free(cmd);

        //write
        for ( frame=i_frame_start; frame<i_frame_total; frame++ )
        {
            frm = avs_h.func.avs_get_frame( avs_h.clip, frame );
            const char *err = avs_h.func.avs_clip_get_error( avs_h.clip );
            if( err )
            {
                fprintf( stderr, "avs [error]: %s occurred while reading frame %d\n", err, frame );
                goto process_fail;
            }            
            planeY = (char*)(frm->vfb->data + frm->offset);
            for (j=0; j<i_height; j++){
               if( !WriteFile(h_pipeWrite, planeY, i_width, (PDWORD)&i, NULL) )
               {
                   fprintf( stderr, "avs [error]: Error occurred while writing frame %d\n(Maybe x264 closed)\n", frame );
                   goto process_fail;
               }
               planeY += frm->pitch;
            }
            planeU = (char*)(frm->vfb->data + frm->offsetU);
            for (j=0; j<chroma_height; j++){
               if( !WriteFile(h_pipeWrite, planeU, chroma_width, (PDWORD)&i, NULL) )
               {
                   fprintf( stderr, "avs [error]: Error occurred while writing frame %d\n(Maybe x264 closed)\n", frame );
                   goto process_fail;
               }
               planeU += frm->pitchUV;
            } 
            planeV = (char*)(frm->vfb->data + frm->offsetV);
            for (j=0; j<chroma_height; j++){
               if( !WriteFile(h_pipeWrite, planeV, chroma_width, (PDWORD)&i, NULL) )
               {
                   fprintf( stderr, "avs [error]: Error occurred while writing frame %d\n(Maybe x264 closed)\n", frame );
                   goto process_fail;
               }
               planeV += frm->pitchUV;
            } 
            avs_h.func.avs_release_video_frame( frm );
        }
        //close & cleanup
    process_fail:// everything created
        CloseHandle(h_pipeWrite);// h_pipeRead already closed
        WaitForSingleObject(pi_info.hProcess, INFINITE);
        GetExitCodeProcess(pi_info.hProcess,&exitcode);
        CloseHandle(pi_info.hProcess);
        goto avs_cleanup;// pipes already closed
    pipe_fail://pipe created but failed after that
        CloseHandle(h_pipeRead);
        CloseHandle(h_pipeWrite);
    avs_fail://avs enviormnet created but failed after that
        exitcode = -1;
    avs_cleanup:
        avs_h.func.avs_release_clip( avs_h.clip );
        if( avs_h.func.avs_delete_script_environment )
            avs_h.func.avs_delete_script_environment( avs_h.env );
        FreeLibrary( avs_h.library );
    }
    else
    {
        printf("\n"
               "avs4x264mod - simple Avisynth pipe tool for x264\n"
               "Version: %d.%d.%d.%d, built on %s, %s\n\n", VERSION_MAJOR, VERSION_MINOR, VERSION_BUGFIX, VERSION_GIT, __DATE__, __TIME__);
        printf("Usage: avs4x264mod [avs4x264mod options] [x264 options] <input>.avs\n"        , argv[0]);
        printf("Options:\n");
        printf(" -L, --x264-binary <file>   User defined x264 binary path. [Default=\"%s\"]\n", DEFAULT_BINARY_PATH);
        printf("     --seek-mode <string>   Set seek mode when using --seek. [Default=\"fast\"]\n"
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
        return -1;
    }
    return exitcode;
}
