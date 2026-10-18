/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef HUSK_HOST_CONTROL_H
#define HUSK_HOST_CONTROL_H
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <spawn.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

/* Check the opened object, avoiding pathname check/open races. */
static inline FILE *tl_host_control_open(const char *path)
{
    int fd = open(path, O_RDONLY | O_NOFOLLOW | O_CLOEXEC | O_NONBLOCK);
    if (fd < 0) return NULL;
    struct stat st;
    if (fstat(fd, &st) < 0) { int e = errno; close(fd); errno = e; return NULL; }
    if ((!S_ISREG(st.st_mode) && !S_ISFIFO(st.st_mode)) ||
        st.st_uid != geteuid() || (st.st_mode & 0022)) {
        close(fd); errno = EPERM; return NULL;
    }
    int flags = fcntl(fd, F_GETFL);
    if (flags < 0 || fcntl(fd, F_SETFL, flags & ~O_NONBLOCK) < 0) {
        int e = errno; close(fd); errno = e; return NULL;
    }
    FILE *f = fdopen(fd, "r");
    if (!f) { int e = errno; close(fd); errno = e; }
    return f;
}

/* Discard an entire overlong/NUL-containing command, never execute a prefix. */
static inline int tl_host_control_line(FILE *f, char *line, size_t size)
{
    if (size < 2) { errno = EINVAL; return 0; }
    for (;;) {
        size_t used = 0;
        int c, invalid = 0;
        while ((c = fgetc(f)) != EOF && c != '\n') {
            if (!c || used + 1 >= size) invalid = 1;
            if (!invalid) line[used++] = (char)c;
        }
        if (c == EOF && !used && !invalid) return 0;
        if (invalid) { fprintf(stderr, "ctl: invalid or overlong command ignored\n"); continue; }
        line[used] = 0;
        return 1;
    }
}

/* The rest of a shot line is one literal path; spaces and quotes are data. */
static inline int tl_host_shot_path(const char *line, char *path, size_t size)
{
    if (strncmp(line, "shot", 4) || (line[4] && !isspace((unsigned char)line[4]))) return 0;
    if (!size) { errno = EINVAL; return 1; }
    path[0] = 0;
    const char *p = line + 4;
    while (*p == ' ' || *p == '\t') p++;
    size_t len = strcspn(p, "\r\n");
    if (!len || len >= size) { errno = len ? ENAMETOOLONG : EINVAL; return 1; }
    memcpy(path, p, len); path[len] = 0;
    return 1;
}

static inline int tl_host_absolute_path(char *out, size_t size, const char *path)
{
    if (!path || !*path) { errno = EINVAL; return -1; }
    int len;
    if (path[0] == '/') len = snprintf(out, size, "%s", path);
    else {
        char cwd[PATH_MAX];
        if (!getcwd(cwd, sizeof(cwd))) return -1;
        len = snprintf(out, size, "%s/%s", cwd, path);
    }
    if (len < 0 || (size_t)len >= size) { errno = ENAMETOOLONG; return -1; }
    return 0;
}

static inline int tl_host_screenshot(const char *frame_dir, const char *path)
{
    char frames[PATH_MAX], input[PATH_MAX], output[PATH_MAX];
    if (tl_host_absolute_path(frames, sizeof(frames), frame_dir) ||
        tl_host_absolute_path(output, sizeof(output), path)) return -1;
    int len = snprintf(input, sizeof(input), "%s/latest.bmp", frames);
    if (len < 0 || (size_t)len >= sizeof(input)) { errno = ENAMETOOLONG; return -1; }
    char *const argv[] = { "/usr/bin/sips", "-s", "format", "png", input, "--out", output, NULL };
    posix_spawn_file_actions_t actions;
    int err = posix_spawn_file_actions_init(&actions);
    if (err) { errno = err; return -1; }
    err = posix_spawn_file_actions_addopen(&actions, STDOUT_FILENO, "/dev/null", O_WRONLY, 0);
    if (!err) err = posix_spawn_file_actions_adddup2(&actions, STDOUT_FILENO, STDERR_FILENO);
    pid_t pid;
    extern char **environ;
    if (!err) err = posix_spawn(&pid, "/usr/bin/sips", &actions, NULL, argv, environ);
    posix_spawn_file_actions_destroy(&actions);
    if (err) { errno = err; return -1; }
    int status;
    while (waitpid(pid, &status, 0) < 0) { if (errno != EINTR) return -1; }
    if (!WIFEXITED(status) || WEXITSTATUS(status)) { errno = EIO; return -1; }
    return 0;
}
#endif
