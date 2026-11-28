// PID 1 of the WibWob-DOS machine: the first and only program the Linux kernel starts.
// It mounts the kernel's file systems, makes the console the desktop's terminal, runs
// /wwdos/wwdos there, and powers the machine off when the desktop exits (Alt-X).
//
// Built static for aarch64 by unkernel/build.sh and packed as /init of the initramfs.

#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mount.h>
#include <sys/reboot.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

static void mnt(const char *src, const char *dir, const char *type, const char *opts) {
    mkdir(dir, 0755);
    if (mount(src, dir, type, 0, opts) != 0 && errno != EBUSY)
        fprintf(stderr, "init: mount %s: %s\n", dir, strerror(errno));
}

// The value of NAME=value on the kernel command line, or null.
static char *cmdlineValue(const char *name) {
    static char line[4096];
    int fd = open("/proc/cmdline", O_RDONLY);
    if (fd < 0) return NULL;
    ssize_t n = read(fd, line, sizeof(line) - 1);
    close(fd);
    if (n <= 0) return NULL;
    line[n] = '\0';
    size_t len = strlen(name);
    for (char *tok = strtok(line, " \n"); tok; tok = strtok(NULL, " \n"))
        if (strncmp(tok, name, len) == 0 && tok[len] == '=') return tok + len + 1;
    return NULL;
}

int main(void) {
    mnt("devtmpfs", "/dev", "devtmpfs", NULL);
    mnt("proc", "/proc", "proc", NULL);
    mnt("sysfs", "/sys", "sysfs", NULL);
    mnt("tmpfs", "/tmp", "tmpfs", "mode=1777");
    mnt("tmpfs", "/run", "tmpfs", "mode=755");
    mnt("devpts", "/dev/pts", "devpts", "gid=5,mode=620,ptmxmode=666"); // terminal windows
    mnt("tmpfs", "/dev/shm", "tmpfs", "mode=1777");
    mkdir("/root", 0700);

    setenv("PATH", "/bin:/wwdos", 1);
    setenv("HOME", "/root", 1);
    setenv("TERM", "xterm-256color", 1);
    setenv("COLORTERM", "truecolor", 1);
    setenv("LANG", "C.UTF-8", 1);
    setenv("SHELL", "/bin/sh", 1);
    setenv("WWDOS_REPO", "/wwdos", 1);
    setenv("WIBWOB_REPO_ROOT", "/wwdos", 1);
    setenv("WWDOS_MIDI", "off", 1);
    // The mixer has no ALSA output yet: run it silent at real time.
    setenv("WWDOS_SOUND_OUT", "null", 1);

    // wwdos.shell=1 on the kernel command line starts a shell on the console instead.
    char *shell = cmdlineValue("wwdos.shell");
    int wantShell = shell && shell[0] == '1';

    pid_t child = fork();
    if (child == 0) {
        setsid();
        int tty = open("/dev/hvc0", O_RDWR);
        if (tty < 0) tty = open("/dev/console", O_RDWR);
        if (tty >= 0) {
            ioctl(tty, TIOCSCTTY, 1); // SIGWINCH on resize, job control for the desktop
            dup2(tty, 0);
            dup2(tty, 1);
            // The desktop draws on the terminal; its log lines would tear the screen.
            int log = wantShell ? -1 : open("/tmp/wwdos.log", O_WRONLY | O_CREAT | O_APPEND, 0644);
            dup2(log >= 0 ? log : tty, 2);
            if (log > 2) close(log);
            if (tty > 2) close(tty);
        }
        if (chdir("/wwdos") != 0) perror("init: /wwdos");
        if (wantShell) execl("/bin/sh", "sh", (char *)NULL);
        else execl("/wwdos/wwdos", "wwdos", (char *)NULL);
        perror("init: exec");
        _exit(127);
    }

    // Reap everything; the machine lives as long as the desktop.
    for (;;) {
        int status;
        pid_t pid = wait(&status);
        if (pid == child || (pid < 0 && errno == ECHILD)) break;
    }
    sync();
    reboot(RB_POWER_OFF);
    for (;;) pause();
}
