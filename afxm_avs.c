#include "avs4x264mod.h"

avs_hnd_t avs_h;
AVS_Value arg;
AVS_Value res;
unsigned int frame, len;

/* load the library and functions we require from it */
int avs_load_library( avs_hnd_t *h )
{
	h->library = LoadLibrary( "avisynth" );
	if ( !h->library )
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

float get_avs_version( avs_hnd_t avs_h )
{
	if ( !avs_h.func.avs_function_exists( avs_h.env, "VersionNumber" ) )
	{
		fprintf( stderr, "avs [error]: VersionNumber does not exist\n" );
		return -1;
	}
	AVS_Value ver = avs_h.func.avs_invoke( avs_h.env, "VersionNumber", avs_new_value_array( NULL, 0 ), NULL );
	if ( avs_is_error( ver ) )
	{
		fprintf( stderr, "avs [error]: Unable to determine avisynth version: %s\n", avs_as_error( ver ) );
		return -1;
	}
	if ( !avs_is_float( ver ) )
	{
		fprintf( stderr, "avs [error]: VersionNumber did not return a float value\n" );
		return -1;
	}
	float ret = avs_as_float( ver );
	avs_h.func.avs_release_value( ver );
	return ret;
}

AVS_Value update_clip( avs_hnd_t avs_h, const AVS_VideoInfo *vi, AVS_Value res, AVS_Value release )
{
	avs_h.func.avs_release_clip( avs_h.clip );
	avs_h.clip = avs_h.func.avs_take_clip( res, avs_h.env );
	avs_h.func.avs_release_value( release );
	vi = avs_h.func.avs_get_video_info( avs_h.clip );
	return res;
}

int LoadAVSFile(video_info_t *VideoInfo, cmd_t *cmd_options)
{
	float avs_version;
	//avs open
	if ( avs_load_library( &avs_h ) )
	{
		fprintf( stderr, "avs [error]: failed to load avisynth\n" );
		return ERR_AVS_NOTFOUND;
	}
	avs_h.env = avs_h.func.avs_create_script_environment( AVS_INTERFACE_YV12 );
	if ( !avs_h.env )
	{
		fprintf( stderr, "avs [error]: failed to initiate avisynth\n" );
		return ERR_AVS_FAIL;
	}
	fprintf( stderr, "avs [info]: Trying to open %s\n", VideoInfo->infile );
	arg = avs_new_value_string( VideoInfo->infile );

	res = avs_h.func.avs_invoke( avs_h.env, "Import", arg, NULL );
	if ( avs_is_error( res ) )
	{
		fprintf( stderr, "avs [error]: %s\n", avs_as_string( res ) );
		return ERR_AVS_FAIL;
	}

	/* check if the user is using a multi-threaded script and apply distributor if necessary.
		adapted from avisynth's vfw interface */
	AVS_Value mt_test = avs_h.func.avs_invoke( avs_h.env, "GetMTMode", avs_new_value_bool( 0 ), NULL );
	int mt_mode = avs_is_int( mt_test ) ? avs_as_int( mt_test ) : 0;
	avs_h.func.avs_release_value( mt_test );
	if ( mt_mode > 0 && mt_mode < 5 )
	{
		AVS_Value temp = avs_h.func.avs_invoke( avs_h.env, "Distributor", res, NULL );
		avs_h.func.avs_release_value( res );
		res = temp;
	}

	if ( !avs_is_clip( res ) )
	{
		fprintf( stderr, "avs [error]: `%s' didn't return a video clip\n", VideoInfo->infile );
		return ERR_AVS_FAIL;
	}
	avs_h.clip = avs_h.func.avs_take_clip( res, avs_h.env );
	avs_version = get_avs_version( avs_h );
	fprintf( stdout, "avs [info]: Avisynth version: %.2f\n", avs_version );
	const AVS_VideoInfo *vi = avs_h.func.avs_get_video_info( avs_h.clip );
	if ( !avs_has_video( vi ) )
	{
		fprintf( stderr, "avs [error]: `%s' has no video data\n", VideoInfo->infile );
		return ERR_AVS_FAIL;
	}
	if ( vi->width & 1 || vi->height & 1 )
	{
		fprintf( stderr, "avs [error]: input clip width or height not divisible by 2 (%dx%d)\n",
		         vi->width, vi->height );
		return ERR_AVS_FAIL;
	}
	if ( avs_is_yv12( vi ) )
	{
		VideoInfo->csp = "i420";
		VideoInfo->chroma_width = vi->width >> 1;
		VideoInfo->chroma_height = vi->height >> 1;
		fprintf( stdout, "avs [info]: Video colorspace: YV12\n" );
	}
	else if ( avs_is_yv24( vi ) )
	{
		VideoInfo->csp = "i444";
		VideoInfo->chroma_width = vi->width;
		VideoInfo->chroma_height = vi->height;
		fprintf( stdout, "avs [info]: Video colorspace: YV24\n" );
	}
	else if ( avs_is_yv16( vi ) )
	{
		VideoInfo->csp = "i422";
		VideoInfo->chroma_width = vi->width >> 1;
		VideoInfo->chroma_height = vi->height;
		fprintf( stdout, "avs [info]: Video colorspace: YV16\n" );
	}
	else
	{
		fprintf( stderr, "avs [warning]: Converting input clip to YV12\n" );
		const char *arg_name[2] = { NULL, "interlaced" };
		AVS_Value arg_arr[2] = { res, avs_new_value_bool( cmd_options->Interlaced ) };
		AVS_Value res2 = avs_h.func.avs_invoke( avs_h.env, "ConvertToYV12", avs_new_value_array( arg_arr, 2 ), arg_name );
		if ( avs_is_error( res2 ) )
		{
			fprintf( stderr, "avs [error]: Couldn't convert input clip to YV12\n" );
			return ERR_AVS_FAIL;
		}
			res = update_clip( avs_h, vi, res2, res );
			VideoInfo->csp = "i420";
			VideoInfo->chroma_width = vi->width >> 1;
			VideoInfo->chroma_height = vi->height >> 1;
	}

		if ( !cmd_options->SeekSafe && VideoInfo->i_frame_start && ( cmd_options->QPFile || cmd_options->TCFile ) )
		{
			fprintf( stdout, "avs4x264 [info]: seek-mode=fast with qpfile or timecodes in, freeze first %d %s for fast processing\n", VideoInfo->i_frame_start, VideoInfo->i_frame_start == 1 ? "frame" : "frames" );
			AVS_Value arg_arr[4] = { res, avs_new_value_int( 0 ), avs_new_value_int( VideoInfo->i_frame_start ), avs_new_value_int( VideoInfo->i_frame_start ) };
			AVS_Value res2 = avs_h.func.avs_invoke( avs_h.env, "FreezeFrame", avs_new_value_array( arg_arr, 4 ), NULL );
			if ( avs_is_error( res2 ) )
			{
				fprintf( stderr, "avs [error]: Couldn't freeze first %d %s\n", VideoInfo->i_frame_start, VideoInfo->i_frame_start == 1 ? "frame" : "frames" );
				return ERR_AVS_FAIL;
			}
			res = update_clip( avs_h, vi, res2, res );
		}
		
	avs_h.func.avs_release_value( res );

	VideoInfo->real_width = VideoInfo->i_width = vi->width;
	VideoInfo->i_height = vi->height;
	VideoInfo->i_fps_num = vi->fps_numerator;
	VideoInfo->i_fps_den = vi->fps_denominator;
	VideoInfo->i_frame_total = vi->num_frames;
	VideoInfo->num_frames = vi->num_frames;

	fprintf( stdout,
	         "avs [info]: Video resolution: %dx%d\n"
	         "avs [info]: Video framerate: %d/%d\n"
	         "avs [info]: Video framecount: %d\n",
	         vi->width, vi->height, vi->fps_numerator, vi->fps_denominator, vi->num_frames );
}

void avs_cleanup()
{
	avs_h.func.avs_release_clip( avs_h.clip );
	if ( avs_h.func.avs_delete_script_environment )
		avs_h.func.avs_delete_script_environment( avs_h.env );
	FreeLibrary( avs_h.library );
}
