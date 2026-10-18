/* SPDX-License-Identifier: GPL-2.0-or-later */
#include <assert.h>
#include <errno.h>
#include <sys/wait.h>
static int inject_eintr;
static pid_t interrupted_waitpid(pid_t pid, int *status, int flags)
{
    if (inject_eintr) { inject_eintr = 0; errno = EINTR; return -1; }
    return waitpid(pid, status, flags);
}
#define waitpid interrupted_waitpid
#include "../../host-control.h"
#undef waitpid

int main(int argc, char **argv)
{
    assert(argc == 2 && chdir(argv[1]) == 0);
    assert(mkdir("frames with ' quote", 0700) == 0);
    unsigned char bmp[58] = {0};
    bmp[0]='B'; bmp[1]='M'; bmp[2]=58; bmp[10]=54; bmp[14]=40;
    bmp[18]=1; bmp[22]=1; bmp[26]=1; bmp[28]=24; bmp[34]=4;
    bmp[54]=0x20; bmp[55]=0x80; bmp[56]=0xe0;
    FILE *f=fopen("frames with ' quote/latest.bmp", "wb"); assert(f);
    assert(fwrite(bmp, 1, sizeof(bmp), f) == sizeof(bmp)); assert(!fclose(f));
    const char *name="literal name';touch INJECTED;#.png";
    char command[512], path[400];
    assert(snprintf(command, sizeof(command), "shot %s", name) > 0);
    assert(tl_host_shot_path(command, path, sizeof(path)) && !strcmp(path, name));
    inject_eintr=1;
    assert(tl_host_screenshot("frames with ' quote", path) == 0 && !inject_eintr);
    assert(access("INJECTED", F_OK) == -1);
    unsigned char signature[8];
    f=fopen(name,"rb"); assert(f && fread(signature,1,8,f)==8); fclose(f);
    assert(!memcmp(signature, "\x89PNG\r\n\x1a\n", 8));
    assert(!tl_host_screenshot("frames with ' quote", "-literal.png"));
    assert(access("-literal.png", F_OK) == 0);
    /* sips returns zero for some missing inputs; use an unwritable output parent. */
    assert(tl_host_screenshot("frames with ' quote", "/dev/null/invalid.png") == -1);
    memset(command, 'x', sizeof(command)); memcpy(command, "shot ", 5); command[505]=0;
    assert(tl_host_shot_path(command,path,sizeof(path)) && !path[0] && errno==ENAMETOOLONG);
    char long_dir[PATH_MAX+20]; memset(long_dir,'a',sizeof(long_dir)-1); long_dir[sizeof(long_dir)-1]=0;
    assert(tl_host_screenshot(long_dir,"unused.png")==-1 && errno==ENAMETOOLONG);
    int fd=open("owned.ctl",O_WRONLY|O_CREAT|O_EXCL,0600); assert(fd>=0); close(fd);
    f=tl_host_control_open("owned.ctl"); assert(f); fclose(f);
    assert(!chmod("owned.ctl",0666)); assert(!tl_host_control_open("owned.ctl"));
    assert(!chmod("owned.ctl",0600)); assert(!symlink("owned.ctl","linked.ctl"));
    assert(!tl_host_control_open("linked.ctl")); assert(!tl_host_control_open("frames with ' quote"));
    assert(!mkfifo("pipe.ctl",0600)); f=tl_host_control_open("pipe.ctl"); assert(f); fclose(f);
    f=tmpfile(); assert(f);
    for(int i=0;i<600;i++) fputc('x',f);
    fputs("\nshot bad",f); fputc(0,f); fputs("quit\nwait 1\n",f); rewind(f);
    assert(tl_host_control_line(f,command,sizeof(command)) && !strcmp(command,"wait 1"));
    assert(!tl_host_control_line(f,command,sizeof(command))); fclose(f);
    puts("PASS: literal screenshot paths/PNG, child failure/EINTR, truncation, control ownership/type/modes/symlink and whole-line handling");
    return 0;
}
