// PID 1 of the WibWob-DOS machine: the first and only program the Linux kernel starts.
// It mounts the kernel's file systems, makes the console the desktop's terminal, runs
// /wwdos/wwdos there, and powers the machine off when the desktop exits (Alt-X).
//
// Built static for aarch64 by unkernel/build.sh and packed as /init of the initramfs.

#define _GNU_SOURCE
#include <errno.h>
#include <poll.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mount.h>
#include <sys/reboot.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <linux/vm_sockets.h>
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

// The control bridge: the host (the page, through vm.vsock.connect) reaches the desktop's
// command socket. One vsock connection carries one command line; the answer comes back
// on the same connection, which then closes.
#define CONTROL_PORT 7137

static int desktopSocket(void) {
    struct sockaddr_un un = {.sun_family = AF_UNIX, .sun_path = "/tmp/wwdos.sock"};
    for (int tries = 0; tries < 50; ++tries) { // the desktop binds it a moment after it starts
        int fd = socket(AF_UNIX, SOCK_STREAM, 0);
        if (fd < 0) return -1;
        if (connect(fd, (struct sockaddr *)&un, sizeof(un)) == 0) return fd;
        close(fd);
        usleep(100000);
    }
    return -1;
}

static void writeAll(int fd, const char *buf, size_t len) {
    while (len > 0) {
        ssize_t n = write(fd, buf, len);
        if (n <= 0) return;
        buf += n;
        len -= (size_t)n;
    }
}

static void controlBridge(void) {
    signal(SIGPIPE, SIG_IGN);
    int lfd = socket(AF_VSOCK, SOCK_STREAM, 0);
    struct sockaddr_vm vm = {.svm_family = AF_VSOCK, .svm_cid = VMADDR_CID_ANY, .svm_port = CONTROL_PORT};
    if (lfd < 0 || bind(lfd, (struct sockaddr *)&vm, sizeof(vm)) != 0 || listen(lfd, 8) != 0) _exit(1);
    static char line[1 << 16], buf[1 << 14];
    for (;;) {
        int c = accept(lfd, NULL, NULL);
        if (c < 0) continue;
        size_t len = 0;
        while (len < sizeof(line) - 1 && !memchr(line, '\n', len)) {
            ssize_t n = read(c, line + len, sizeof(line) - 1 - len);
            if (n <= 0) break;
            len += (size_t)n;
        }
        if (len > 0 && line[len - 1] != '\n') line[len++] = '\n';
        int d = len > 0 ? desktopSocket() : -1;
        if (d >= 0) {
            writeAll(d, line, len);
            ssize_t n;
            while ((n = read(d, buf, sizeof(buf))) > 0) writeAll(c, buf, (size_t)n);
            close(d);
        } else if (len > 0) {
            writeAll(c, "err no desktop socket\n", 22);
        }
        close(c);
    }
}

// Environment from the host (wwdos.env=1 on the command line): the page connects to vsock port 7138 and
// writes NAME=value lines, such as an API key the person typed there, then a line with one full stop.
// Waits up to 10 s for the page.
#define ENV_PORT 7138

static void hostEnvironment(void) {
    int lfd = socket(AF_VSOCK, SOCK_STREAM, 0);
    struct sockaddr_vm vm = {.svm_family = AF_VSOCK, .svm_cid = VMADDR_CID_ANY, .svm_port = ENV_PORT};
    if (lfd < 0 || bind(lfd, (struct sockaddr *)&vm, sizeof(vm)) != 0 || listen(lfd, 1) != 0) return;
    struct pollfd p = {.fd = lfd, .events = POLLIN};
    int c = poll(&p, 1, 10000) == 1 ? accept(lfd, NULL, NULL) : -1;
    close(lfd);
    if (c < 0) return;
    static char text[8192];
    size_t len = 0;
    ssize_t n;
    text[0] = '\0';
    while (len < sizeof(text) - 1 && !strstr(text, "\n.\n") && (n = read(c, text + len, sizeof(text) - 1 - len)) > 0) {
        len += (size_t)n;
        text[len] = '\0';
    }
    close(c);
    for (char *line = strtok(text, "\n"); line; line = strtok(NULL, "\n")) {
        char *eq = strchr(line, '=');
        if (!eq || eq == line) continue; // the closing "." too
        *eq = '\0';
        setenv(line, eq + 1, 1);
    }
}

// The network, when the host gave the machine one (eth0): an address by DHCP. /tmp/net.log has the result.
static void network(void) {
    if (access("/sys/class/net/eth0", F_OK) != 0) _exit(0);
    execl("/bin/sh", "sh", "-c",
          "ifconfig lo 127.0.0.1 up; exec udhcpc -i eth0 -n -q -t 8 -s /etc/udhcpc.script >/tmp/net.log 2>&1",
          (char *)NULL);
    _exit(127);
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

    char *env = cmdlineValue("wwdos.env");
    if (env && env[0] == '1') hostEnvironment();

    if (fork() == 0) network();
    if (!wantShell && fork() == 0) controlBridge();

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
