#include "avs4x264mod.h"

int parse_opt(int *_argc, char **argv, cmd_t *cmd_options)
{
	int i;
	int argc = *_argc;
	for (i = 1; i < argc; i++)
	{
		if (!strcmp(argv[i], "--affinity"))
		{
			cmd_options->Affinity = atoi(argv[i + 1]);
			memmove(argv + i, argv + i + 2, (argc - i - 2) * sizeof(char *));
			argc -= 2;
			i--;
		}
		else if (!strcmp(argv[i], "--x264-affinity"))
		{
			cmd_options->x264Affinity = atoi(argv[i + 1]);
			memmove(argv + i, argv + i + 2, (argc - i - 2) * sizeof(char *));
			argc -= 2;
			i--;
		}
		else if (!strcmp(argv[i], "--pipe-buffer"))
		{
			cmd_options->BufferSize = atoi(argv[i + 1]);
			memmove(argv + i, argv + i + 2, (argc - i - 2) * sizeof(char *));
			argc -= 2;
			i--;
		}
		else if (!strcmp(argv[i], "--seek"))
		{
			cmd_options->Seek = atoi(argv[i + 1]);
			memmove(argv + i, argv + i + 2, (argc - i - 2) * sizeof(char *));
			argc -= 2;
			i--;
		}
		else if (!strcmp(argv[i], "--frames"))
		{
			cmd_options->Frames = atoi(argv[i + 1]);
			memmove(argv + i, argv + i + 2, (argc - i - 2) * sizeof(char *));
			argc -= 2;
			i--;
		}
		else if (!strcmp(argv[i], "--seek-mode"))
		{
			if ( !strcasecmp(argv[i + 1], "safe" ) )
			{
				cmd_options->SeekSafe = 1;
			}
			else if ( !strcasecmp(argv[i + 1], "fast" ) )
			{
				cmd_options->SeekSafe = 0;
			}
			else
			{
				fprintf( stderr, "avs4x264 [error]: invalid seek-mode\n" );
				return -1;
			}
			memmove(argv + i, argv + i + 2, (argc - i - 2) * sizeof(char *));
			argc -= 2;
			i--;
		}
		else if (!strcmp(argv[i], "--seek-safe"))
		{
			cmd_options->SeekSafe = 1;
			memmove(argv + i, argv + i + 1, (argc - i - 1) * sizeof(char *));
			argc--;
			i--;
		}
		else if (!strcmp(argv[i], "--pipe-mt"))
		{
			cmd_options->PipeMT = 1;
			memmove(argv + i, argv + i + 1, (argc - i - 1) * sizeof(char *));
			argc--;
			i--;
		}
		else if (!strcmp(argv[i], "--interlaced") || !strcmp(argv[i], "--tff") || !strcmp(argv[i], "--bff"))
		{
			cmd_options->Interlaced = 1;
		}
		else if (!strcmp(argv[i], "--qpfile"))
		{
			cmd_options->QPFile = 1;
		}
		else if (!strcmp(argv[i], "--tcfile-in"))
		{
			cmd_options->TCFile = 1;
		}
		else if (!strcmp(argv[i], "--x264-binary"))
		{
			cmd_options->X264Path = argv[i + 1];
			memmove(argv + i, argv + i + 2, (argc - i - 2) * sizeof(char *));
			argc -= 2;
			i--;
		}
		else if ( !strcmp(argv[i], "--output") || !strcmp(argv[i], "-o") || !strcmp(argv[i], "--audiofile") )
		{
			i++; // prevent output file being detected as input.
		}
		else
		{
			int len = strlen(argv[i]);
			if (len > 4)
			{
				char * extension;
				for(extension = argv[i] + len - 4; extension[0] != '.' && extension > argv[i]; extension--);
				if(strcasecmp(extension, ".avs") == 0)
				{
					cmd_options->InFileType = IFT_AVS;
					cmd_options->InFile = argv[i];
				}
				else if(strcasecmp(extension, ".d2v") == 0)
				{
					cmd_options->InFileType = IFT_D2V;
					cmd_options->InFile = argv[i];
				}
				else if(strcasecmp(extension, ".dga") == 0)
				{
					cmd_options->InFileType = IFT_DGA;
					cmd_options->InFile = argv[i];
				}
				else if(strcasecmp(extension, ".dgi") == 0)
				{
					cmd_options->InFileType = IFT_DGI;
					cmd_options->InFile = argv[i];
				}
				// Go use dgavc instead
				else if(strcasecmp(extension, ".m2ts") == 0
					||	strcasecmp(extension, ".ogv") == 0
					||	strcasecmp(extension, ".ogm") == 0
					||	strcasecmp(extension, ".mp4") == 0
					||	strcasecmp(extension, ".m4v") == 0
					||	strcasecmp(extension, ".mkv") == 0
					||	strcasecmp(extension, ".mov") == 0
					||	strcasecmp(extension, ".flv") == 0
					||	strcasecmp(extension, ".wmv") == 0
					)
				{
					color_printf( "avs4x264 [info]: I'd recommend open %s using x264 directly, when opening %s type.\n", argv[i], extension);
					cmd_options->InFileType = 0;
					cmd_options->InFile = argv[i];
					//TODO: support FFV or DSS2
				}
				else if(strcasecmp(extension, ".mpeg") == 0
					||	strcasecmp(extension, ".vob") == 0
					||	strcasecmp(extension, ".m2v") == 0
					||	strcasecmp(extension, ".mpg") == 0
					||	strcasecmp(extension, ".ts") == 0
					||	strcasecmp(extension, ".tp") == 0
					||	strcasecmp(extension, ".ps") == 0
					)
				{
					color_printf( "avs4x264 [info]: I'd recommend index %s using corresponding DG-tools, when opening %s type.\n", argv[i], extension);
					cmd_options->InFileType = 0;
					cmd_options->InFile = argv[i];
					//TODO: support FFV or DSS2
				}
			}
		}
	}
	*_argc = argc;
}
/*
int main(int argc, char **argv)
{
	cmd_t *cmd_options = (cmd_t *)malloc(sizeof(cmd_t));
	memset(cmd_options, 0, sizeof(cmd_t));
	parse_opt(&argc, argv, cmd_options);
	printf("AFF=%d XAFF=%d BUFF=%d SS=%d PP=%d\n",
		   cmd_options->Affinity,
		   cmd_options->x264Affinity,
		   cmd_options->BufferSize,
		   cmd_options->SeekSafe,
		   cmd_options->PipeMT);
	int i;
	for (i = 1; i < argc; i++)
	{
		puts(argv[i]);
	}
}
*/
