/* s. rannou <mxs@sbrk.org> */

#define _BSD_SOURCE
#define _XOPEN_SOURCE

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <pwd.h>

void usage(char *progname);

void daemonize()
{
    int pid;

    /* let the parent die so we become orphan */
    pid = fork();
    if (pid < 0) {
        fprintf(stderr, "unable to fork: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }
    if (pid > 0) {
        exit(EXIT_SUCCESS);
    }

    /* get a new process group */
    if (setsid() == -1) {
        fprintf(stderr, "unable to get a new session id: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }
}

uid_t get_uid_for_user(char *user)
{
    struct passwd *pw_user;

    if (user == NULL) {
        return getuid();
    }

    pw_user = getpwnam(user);
    if (!pw_user) {
        fprintf(stderr, "unable to find the user %s: %s\n", user, strerror(errno));
        exit(EXIT_FAILURE);
        return -1;
    }

    return pw_user->pw_uid;
}

int redirect_outputs(char *path_stdin, char *path_stdout, char *path_stderr)
{
    int fdin;
    int fdout;
    int fderr;

    /* looks complicated but attempts to always print something on error */

    if (path_stdin) {
	fdin = open(path_stdin, O_RDONLY | O_CREAT, 0644);
	if (fdin == -1) {
	    fprintf(stderr, "unable to open stdin %s: %s\n", path_stdin, strerror(errno));
	    exit(EXIT_FAILURE);
	    return -1;
	}
	close(0);
	dup(fdin);
    }

    if (path_stdout) {
	fdout = open(path_stdout, O_WRONLY | O_CREAT | O_APPEND, 0644);
	if (fdout == -1) {
	    fprintf(stderr, "unable to open stdin %s: %s\n", path_stdout, strerror(errno));
	    exit(EXIT_FAILURE);
	    return -1;
	}
	close(1);
	dup(fdout);
    }

    if (path_stderr) {
	fderr = open(path_stderr, O_WRONLY | O_CREAT | O_APPEND, 0644);
	if (fderr == -1) {
	    fprintf(stderr, "unable to open stdin %s: %s\n", path_stderr, strerror(errno));
	    exit(EXIT_FAILURE);
	    return -1;
	}
	close(2);
	dup(fderr);
    }

    return 0;
}

void mutate(uid_t uid)
{
    /* do not mutate in what we already are */
    if (uid != getuid()) {
        if (setuid(uid) == -1) {
            fprintf(stderr, "unable to setuid to user %d: %s\n", uid, strerror(errno));
            exit(EXIT_FAILURE);
        }
    }
}

void jail(char *dest)
{
    /* at this point, we have already chdired to dest */
    if (chroot(".") == -1) {
        fprintf(stderr, "unable to chroot to %s: %s\n", dest, strerror(errno));
        exit(EXIT_FAILURE);
    }
}

void run(char *argv[], char *env[], char *pidfile)
{
    char pidstr[32], *pidpos;
    int fd, to_write, ret;

    if (pidfile != NULL) {
	fd = open(pidfile, O_CREAT | O_RDWR, 0640);
	if (fd == -1) {
	    fprintf(stderr, "unable to open pidfile %s: %s\n", pidfile, strerror(errno));
	    exit(EXIT_FAILURE);
	}
	if (lockf(fd, F_TLOCK, 0) == -1) {
	    fprintf(stderr, "unable to lock pidfile %s: %s\n", pidfile, strerror(errno));
	    exit(EXIT_FAILURE);
	}

	/* write pid to pidfile */
	to_write = snprintf(pidstr, sizeof(pidstr), "%d\n", getpid());
	pidpos = pidstr;
	while (to_write > 0) {
	    ret = write(fd, pidpos, to_write);
	    if (ret == -1) {
		fprintf(stderr, "unable to write pid to pidfile %s: %s\n", pidfile, strerror(errno));
		exit(EXIT_FAILURE);
	    }
	    pidpos += ret;
	    to_write -= ret;
	}
    }

    if (execve(argv[0], argv, env) == -1) {
        unlink(pidfile);
        fprintf(stderr, "unable to exec command %s: %s\n", argv[0], strerror(errno));
        exit(EXIT_FAILURE);
    }
}

int main(int argc, char *argv[], char *env[])
{
    char *user = NULL;
    char *dir = NULL;
    char *pidfile = NULL;
    char *path_stdin = NULL;
    char *path_stderr = NULL;
    char *path_stdout = NULL;
    char *progname = argv[0];
    int has_jail_opt = 0;
    int has_debug_opt = 0;
    int opt;
    uid_t uid;

    while ((opt = getopt(argc, argv, "djp:u:c:i:o:e:")) != -1) {
        switch (opt) {
        case 'u':
            user = optarg;
            break;

        case 'd':
            has_debug_opt = 1;
            break;

        case 'p':
            pidfile = optarg;
            break;

        case 'c':
            dir = optarg;
            break;

        case 'j':
            has_jail_opt = 1;
            break;

	case 'i':
	    path_stdin = optarg;
	    break;

	case 'o':
	    path_stdout = optarg;
	    break;

	case 'e':
	    path_stderr = optarg;
	    break;

        default:
            usage(progname);
            /* NEVER REACHED */
        }
    }

    if (has_jail_opt && !dir) {
        usage(progname);
    }

    argc -= optind;
    argv += optind;

    if (argc <= 0) {
        usage(progname);
        /* NEVER REACHED */
    }

    if (dir) {
        if (chdir(dir) == -1) {
            fprintf(stderr, "unable to chdir to %s: %s\n", dir, strerror(errno));
            exit(EXIT_FAILURE);
        }
    }

    uid = get_uid_for_user(user);

    if (has_jail_opt) {
        jail(dir);
    }

    redirect_outputs(path_stdin, path_stdout, path_stderr);

    mutate(uid);

    if (!has_debug_opt) {
        daemonize();
    }

    run(argv, env, pidfile);

    exit(EXIT_SUCCESS);
}

void usage(char *progname)
{
    fprintf(stderr,
            "usage : %s [-p PIDFILE] [-i STDIN] [-o STDOUT] [-e STDERR] [-d] [-j] [-u USER] [-c CHDIR] -- COMMAND [ARGS]\n"
            "\n"
            "-p\t\tuse PIDFILE (relative to CHDIR) as a lock file in which the PID of COMMAND is written\n"
	    "-i\t\tuse FILE as stdin\n"
	    "-o\t\tuse FILE as stdout\n"
	    "-e\t\tuse FILE as stderr\n"
            "-d\t\tdebug mode, do not daemonize to print errors\n"
            "-j\t\tjail the command with chroot, has *no* effect if you aren't root\n"
            "-u\t\texecute COMMAND as USER\n"
            "-c\t\tchange the working directory of COMMAND to CHDIR\n",
            progname);
    exit(EXIT_FAILURE);
}
