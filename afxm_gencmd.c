#include "avs4x264mod.h"

char * x264_generate_command(cmd_t *cmdopt, x264_cmd_t *xcmdopt, video_info_t *vi)
{
	int i;
	char *cmd, *buf, *p_cmd;
	int b_add_fps    = 1;
	int b_add_csp    = 1;
	int b_add_res    = 1;
	int len = (unsigned int)strrchr(xcmdopt->argv[0], '\\');
	char *x264_binary;
	x264_binary = DEFAULT_BINARY_PATH;
	buf = malloc(1024);
	if (len)
	{
		len -= (unsigned int)xcmdopt->argv[0];
		strncpy(buf, xcmdopt->argv[0], len);
		buf[len] = '\\';
		buf[len + 1] = 0;
	}
	else
	{
		*buf = 0;
	}
	p_cmd = cmd = malloc(8192);
	if(cmdopt->X264Path)
		x264_binary = cmdopt->X264Path;
	
	if ( cmdopt->TCFile )
		b_add_fps = 0;
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
			if ( !strcmp(xcmdopt->argv[i], "--input-depth") )
			{
				if ( strcmp(xcmdopt->argv[++i], "8") )
				{
					vi->real_width >>= 1;
					fprintf( stdout, "avs4x264 [info]: High bit depth detected, resolution corrected\n" );
				}
			}
			else if ( strcmp(xcmdopt->argv[i], "--input-depth=8") )
			{
				vi->real_width >>= 1;
				fprintf( stdout, "avs4x264 [info]: High bit depth detected, resolution corrected\n" );
			}
			break;
		}
	}

	sprintf(cmd, " %s%s\" - ", buf, x264_binary);

	/* skip invalid path name when both avs4x264mod and x264 binary is given by full path */
	char *cmd_tmp;
	cmd_tmp = malloc(8192);
	int cmd_len = strlen(cmd);
	cmd = strrchr( cmd, '\"' );                                              /* find the end of x264 binary path */
	while ( cmd >= p_cmd && cmd[0] != ':' )                         /* find if drive number is given */
		cmd--;
	while ( cmd > p_cmd && cmd[0] != '\\' && cmd[0] != '/' )    /* if find drive number, skip invalid path before it */
		cmd--;
	cmd[0] = '"';                                                               /* insert '"' before processed path */
	strcpy(cmd_tmp, cmd);
	free(p_cmd);
	cmd = cmd_tmp;

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
	free(buf);
	return cmd;
}