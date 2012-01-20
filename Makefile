CC=gcc -s -O3 -c -Wl,--large-address-aware
objs=avs4x264mod.o afxm_avs.o afxm_piper.o afxm_gencmd.o afxm_getopt.o

all: avs4x264mode.exe

$(objs): avs4x264mod.h

avs4x264mode.exe: $(objs)
	gcc -o $@ $(objs) -Wl,--large-address-aware

.c.o:
	$(CC) $<

version.h: .git/FETCH_HEAD
	VER=`git rev-list HEAD | wc -l`
	echo "#define VERSION_GIT $VER" > version.h


clean:
	del *.o
