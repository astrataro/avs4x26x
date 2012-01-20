#include "avs4x264mod.h"

char * x264_generate_command(cmd_t *cmdopt, x264_cmd_t *xcmdopt, video_info_t *vi)
{
	int i;
	char *cmd, *buf;
	int b_add_fps    = 1;
	int b_add_csp    = 1;
	int b_add_res    = 1;
	int len = (unsigned int)strrchr(xcmdopt->argv[0], '\\');
	char *x264_binary;
	x264_binary = DEFAULT_BINARY_PATH;
	if (len)
	{
		len -= (unsigned int)xcmdopt->argv[0];
		buf = malloc(1024);
		strncpy(buf, xcmdopt->argv[0], len);
		buf[len] = '\\';
		buf[len + 1] = 0;
	}
	else
	{
		buf = malloc(1024);
		*buf = 0;
	}
	cmd = malloc(8192);
	
	for (i = 1; i < xcmdopt->argc; i++)
	{
		if ( !strncmp(xcmdopt->argv[i], "--x264-binary", 13) || !strncmp(xcmdopt->argv[i], "-L", 2) )
		{
			if ( !strcmp(xcmdopt->argv[i], "--x264-binary") || !strcmp(xcmdopt->argv[i], "-L") )
			{
				x264_binary = xcmdopt->argv[i + 1];
				for (; i < xcmdopt->argc - 2; i++)
					xcmdopt->argv[i] = xcmdopt->argv[i + 2];
				xcmdopt->argc -= 2;
			}
			else if ( !strncmp(xcmdopt->argv[i], "--x264-binary=", 14) )
			{
				x264_binary = xcmdopt->argv[i] + 14;
				for (; i < xcmdopt->argc - 1; i++)
					xcmdopt->argv[i] = xcmdopt->argv[i + 1];
				xcmdopt->argc--;
			}
			else if ( !strncmp(xcmdopt->argv[i], "-L=", 3) )
			{
				x264_binary = xcmdopt->argv[i] + 3;
				for (; i < xcmdopt->argc - 1; i++)
					xcmdopt->argv[i] = xcmdopt->argv[i + 1];
				xcmdopt->argc--;
			}
			else                           /* else xcmdopt->argv[i] should have structure like -Lx264 */
			{
				x264_binary = xcmdopt->argv[i] + 2;
				for (; i < xcmdopt->argc - 1; i++)
					xcmdopt->argv[i] = xcmdopt->argv[i + 1];
				xcmdopt->argc--;
			}
			break;
		}
	}
	if ( cmdopt->TCFile )
	{
		b_add_fps = 0;
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
			if ( !strcmp(xcmdopt->argv[i], "--input-depth") )
			{
				if ( strcmp(xcmdopt->argv[++i], "8") )
				{
					vi->i_width >>= 1;
					fprintf( stdout, "avs4x264 [info]: High bit depth detected, resolution corrected\n" );
				}
			}
			else if ( strcmp(xcmdopt->argv[i], "--input-depth=8") )
			{
				vi->i_width >>= 1;
				fprintf( stdout, "avs4x264 [info]: High bit depth detected, resolution corrected\n" );
			}
			break;
		}
	}

	sprintf(cmd, "%s%s\" - ", buf, x264_binary);

	/* skip invalid path name when both avs4x264mod and x264 binary is given by full path */
	int p_cmd = (int)cmd;
	char *cmd_tmp;
	cmd_tmp = malloc(8192);
	int cmd_len = strlen(cmd);
	cmd = strrchr( cmd, '\"' );                                              /* find the end of x264 binary path */
	while ( strlen(cmd) < cmd_len && *(cmd) != ':' )                         /* find if drive number is given */
		cmd--;
	while ( strlen(cmd--) < cmd_len && *(cmd) != '\\' && *(cmd) != '/' );    /* if find drive number, skip invalid path before it */
	*cmd = '"';                                                               /* insert '"' before processed path */
	strcpy(cmd_tmp, cmd);
	cmd = (char *)p_cmd;
	strcpy(cmd, cmd_tmp);
	free(cmd_tmp);

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
		sprintf(buf, " --input-res %dx%d", vi->i_width, vi->i_height);
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