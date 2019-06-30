#include "avs4x264mod.h"

char * x264_generate_command(cmd_t *cmdopt, x264_cmd_t *xcmdopt, video_info_t *vi)
{
	int i;
	char *cmd, *buf;
	int b_add_fps      = 1;
	int b_add_csp      = 1;
	int b_add_res      = 1;
	int b_add_depth    = 1;
	int b_add_timebase = 0;
	char *x264_binary;
	x264_binary = DEFAULT_BINARY_PATH;
	buf = malloc(1024);
	buf[0] = 0;
	cmd = malloc(8192);
	if(cmdopt->X264Path)
		x264_binary = cmdopt->X264Path;
	
	if ( cmdopt->TCFile )
	{
		b_add_fps = 0;
		b_add_timebase = 1;
		for (i = 1; i < xcmdopt->argc; i++)
		{
			if ( !strncmp(xcmdopt->argv[i], "--timebase", 10) )
			{
				b_add_timebase = 0;
				break;
			}
		}
	}
	else
	{
		for (i = 1; i < xcmdopt->argc; i++)
		{
			if ( !strncmp(xcmdopt->argv[i], "--fps", 5) )
			{
				b_add_fps = 0;
				break;
			}
		}
	}
	for (i = 1; i < xcmdopt->argc; i++)
	{
		if ( !strncmp(xcmdopt->argv[i], "--input-csp", 11) )
		{
			b_add_csp = 0;
			break;
		}
	}
	for (i = 1; i < xcmdopt->argc; i++)
	{
		if ( !strncmp(xcmdopt->argv[i], "--input-res", 11) )
		{
			b_add_res = 0;
			break;
		}
	}
	for (i = 1; i < xcmdopt->argc; i++)
	{
		if ( !strncmp(xcmdopt->argv[i], "--input-depth", 13) )
		{
			b_add_depth = 0;
			if ( !strcmp(xcmdopt->argv[i], "--input-depth") )
			{
				if ( strcmp(xcmdopt->argv[++i], "8") && vi->bpc == 8 )
				{
					vi->real_width >>= 1;
					fprintf( stdout, "avs4x264 [info]: High bit depth detected, resolution corrected\n" );
				}
			}
			else if ( strcmp(xcmdopt->argv[i], "--input-depth=8") && vi->bpc == 8 )
			{
				vi->real_width >>= 1;
				fprintf( stdout, "avs4x264 [info]: High bit depth detected, resolution corrected\n" );
			}
			break;
		}
	}

	sprintf(cmd, "\"%s\" - ", x264_binary);

	for (i = 1; i < xcmdopt->argc; i++)
	{
		if (vi->infile != xcmdopt->argv[i])
		{
			if (strrchr(xcmdopt->argv[i], ' '))
			{
				strcat(cmd, "\"");
				strcat(cmd, xcmdopt->argv[i]);
				strcat(cmd, "\"");
			}
			else
				strcat(cmd, xcmdopt->argv[i]);
			if (i < xcmdopt->argc - 1)
				strcat(cmd, " ");
		}
	}

	sprintf(buf, " --frames %d", vi->i_encode_frames);
	strcat(cmd, buf);
	strcat(cmd, xcmdopt->extra);

	if ( b_add_fps )
	{
		sprintf(buf, " --fps %d/%d", vi->i_fps_num, vi->i_fps_den);
		strcat(cmd, buf);
	}
	if ( b_add_timebase )
	{
		sprintf(buf, " --timebase %d", vi->i_fps_den);
		strcat(cmd, buf);
	}
	if ( b_add_res )
	{
		sprintf(buf, " --input-res %dx%d", vi->real_width, vi->i_height);
		strcat(cmd, buf);
	}
	if ( b_add_csp )
	{
		sprintf(buf, " --input-csp %s", vi->csp);
		strcat(cmd, buf);
	}
	if ( b_add_depth )
	{
		sprintf(buf, " --input-depth %d", vi->bpc);
		strcat(cmd, buf);
	}
	if (cmdopt->InFileType == IFT_VPH)
	{
		strcat(cmd, " --input-depth 16"); // force 16bit input when using HBVFWSource
	}
	free(buf);
	return cmd;
}