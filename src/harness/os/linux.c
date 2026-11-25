// Based on https://gist.github.com/jvranish/4441299

#ifdef __TINYC__
/* tcc does potentially not have __int128_t, used in /usr/include/bits/link.h:99 */
#define SUPPORT_TRACEBACK 0
#else
#define SUPPORT_TRACEBACK 1
#endif

#define _GNU_SOURCE
#include "harness/config.h"
#include "harness/os.h"
#include <arpa/inet.h>
#include <assert.h>
#include <ctype.h>
#include <dirent.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <ifaddrs.h>
#include <libgen.h>
#include <limits.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>
#include <netinet/in.h>

#if SUPPORT_TRACEBACK
#include <execinfo.h>
#include <link.h>
#endif

#define ARRAY_SIZE(A) (sizeof(A) / sizeof(A[0]))
#define MAX_STACK_FRAMES 64
static char _program_name[1024];

static void* stack_traces[MAX_STACK_FRAMES];
static char name_buf[4096];
static DIR* directory_iterator;

static void* signal_alternative_stack;

#if SUPPORT_TRACEBACK
// The stack, as the frames' addresses (in the executable: its offsets, for addr2line -f -p -e). Without allocating
// or starting a process: after a heap corruption those could hang, the game frozen
static void print_stack_trace(void) {
    int trace_size;

    fputs("\nStack trace (addr2line -f -p -e ", stderr);
    fputs(_program_name, stderr);
    fputs(" with the offsets):\n", stderr);
    trace_size = backtrace(stack_traces, MAX_STACK_FRAMES);
    /* (the first two frames: this function and the handler) */
    if (trace_size > 2) {
        backtrace_symbols_fd(stack_traces + 2, trace_size - 2, STDERR_FILENO);
    }
}
#endif

static void signal_handler(int sig, siginfo_t* siginfo, void* context) {
    sigset_t unblock;

    (void)context;
    // whatever happens in here, the process ends: in 5 s at the latest (alarm's default action)
    signal(SIGALRM, SIG_DFL);
    alarm(5);
    fputs("\n******************\n", stderr);

    switch (sig) {
    case SIGSEGV:
        fputs("Caught SIGSEGV\n", stderr);
        break;
    case SIGINT:
        fputs("Caught SIGINT\n", stderr);
        break;
    case SIGFPE:
        switch (siginfo->si_code) {
        case FPE_INTDIV:
            fputs("Caught SIGFPE: FPE_INTDIV\n", stderr);
            break;
        case FPE_INTOVF:
            fputs("Caught SIGFPE: FPE_INTOVF\n", stderr);
            break;
        case FPE_FLTDIV:
            fputs("Caught SIGFPE: FPE_FLTDIV\n", stderr);
            break;
        case FPE_FLTOVF:
            fputs("Caught SIGFPE: FPE_FLTOVF\n", stderr);
            break;
        case FPE_FLTUND:
            fputs("Caught SIGFPE: FPE_FLTUND\n", stderr);
            break;
        case FPE_FLTRES:
            fputs("Caught SIGFPE: FPE_FLTRES\n", stderr);
            break;
        case FPE_FLTINV:
            fputs("Caught SIGFPE: FPE_FLTINV\n", stderr);
            break;
        case FPE_FLTSUB:
            fputs("Caught SIGFPE: FPE_FLTSUB\n", stderr);
            break;
        default:
            fputs("Caught SIGFPE: Arithmetic Exception\n", stderr);
            break;
        }
        break;
    case SIGILL:
        switch (siginfo->si_code) {
        case ILL_ILLOPC:
            fputs("Caught SIGILL: ILL_ILLOPC\n", stderr);
            break;
        case ILL_ILLOPN:
            fputs("Caught SIGILL: ILL_ILLOPN\n", stderr);
            break;
        case ILL_ILLADR:
            fputs("Caught SIGILL: ILL_ILLADR\n", stderr);
            break;
        case ILL_ILLTRP:
            fputs("Caught SIGILL: ILL_ILLTRP\n", stderr);
            break;
        case ILL_PRVOPC:
            fputs("Caught SIGILL: ILL_PRVOPC\n", stderr);
            break;
        case ILL_PRVREG:
            fputs("Caught SIGILL: ILL_PRVREG\n", stderr);
            break;
        case ILL_COPROC:
            fputs("Caught SIGILL: ILL_COPROC\n", stderr);
            break;
        case ILL_BADSTK:
            fputs("Caught SIGILL: ILL_BADSTK\n", stderr);
            break;
        default:
            fputs("Caught SIGILL: Illegal Instruction\n", stderr);
            break;
        }
        break;
    case SIGTERM:
        fputs("Caught SIGTERM\n", stderr);
        break;
    case SIGABRT:
        fputs("Caught SIGABRT\n", stderr);
        break;
    default:
        break;
    }
    fputs("******************\n", stderr);
#if SUPPORT_TRACEBACK
    print_stack_trace();
#endif
    // the signal again, handled as by default: the process ends as it would have (with a core dump where that
    // is the default), not through exit's handlers, which could hang after a crash
    signal(sig, SIG_DFL);
    sigemptyset(&unblock);
    sigaddset(&unblock, sig);
    sigprocmask(SIG_UNBLOCK, &unblock, NULL);
    raise(sig);
    _exit(128 + sig);
}

void resolve_full_path(char* path, const char* argv0) {
    if (argv0[0] == '/') { // run with absolute path
        strcpy(path, argv0);
    } else { // run with relative path
        if (NULL == getcwd(path, PATH_MAX)) {
            perror("getcwd error");
            return;
        }
        strcat(path, "/");
        strcat(path, argv0);
    }
}

void OS_InstallSignalHandler(char* program_name) {
    resolve_full_path(_program_name, program_name);
#if SUPPORT_TRACEBACK
    // (backtrace loads what it needs the first time: here, not in the handler)
    backtrace(stack_traces, 1);
#endif

    /* setup alternate stack */
    {
        stack_t ss = {};
        signal_alternative_stack = malloc(2 * MINSIGSTKSZ + 4096);
        ss.ss_sp = (void *)((uintptr_t)signal_alternative_stack & ~0xf);
        if (ss.ss_sp == NULL) {
            err(1, "malloc");
        }
        ss.ss_size = 2 * MINSIGSTKSZ;
        ss.ss_flags = 0;

        if (sigaltstack(&ss, NULL) != 0) {
            err(1, "sigaltstack");
        }
    }

    /* register our signal handlers */
    {
        struct sigaction sig_action = {};
        sig_action.sa_sigaction = signal_handler;
        sigemptyset(&sig_action.sa_mask);

        sig_action.sa_flags = SA_SIGINFO | SA_ONSTACK;

        if (sigaction(SIGSEGV, &sig_action, NULL) != 0) {
            err(1, "sigaction");
        }
        if (sigaction(SIGFPE, &sig_action, NULL) != 0) {
            err(1, "sigaction");
        }
        if (sigaction(SIGINT, &sig_action, NULL) != 0) {
            err(1, "sigaction");
        }
        if (sigaction(SIGILL, &sig_action, NULL) != 0) {
            err(1, "sigaction");
        }
        if (sigaction(SIGTERM, &sig_action, NULL) != 0) {
            err(1, "sigaction");
        }
        if (sigaction(SIGABRT, &sig_action, NULL) != 0) {
            err(1, "sigaction");
        }
    }
}

void OS_RemoveSignalHandler(void) {

    /* Unregister our signal handlers */
    {
        struct sigaction sig_action = {};
        sig_action.sa_handler = SIG_DFL;
        sigemptyset(&sig_action.sa_mask);

        if (sigaction(SIGSEGV, &sig_action, NULL) != 0) {
            err(1, "sigaction");
        }
        if (sigaction(SIGFPE, &sig_action, NULL) != 0) {
            err(1, "sigaction");
        }
        if (sigaction(SIGINT, &sig_action, NULL) != 0) {
            err(1, "sigaction");
        }
        if (sigaction(SIGILL, &sig_action, NULL) != 0) {
            err(1, "sigaction");
        }
        if (sigaction(SIGTERM, &sig_action, NULL) != 0) {
            err(1, "sigaction");
        }
        if (sigaction(SIGABRT, &sig_action, NULL) != 0) {
            err(1, "sigaction");
        }
    }

    /* remove alternate stack */
    {
        stack_t ss = {};
        ss.ss_flags = SS_DISABLE;

        if (sigaltstack(&ss, NULL) != 0) {
            err(1, "sigaltstack");
        }
        free(signal_alternative_stack);
        signal_alternative_stack = NULL;
    }
}

char* OS_GetFirstFileInDirectory(char* path) {
    directory_iterator = opendir(path);
    if (directory_iterator == NULL) {
        return NULL;
    }
    return OS_GetNextFileInDirectory();
}

char* OS_GetNextFileInDirectory(void) {
    struct dirent* entry;

    if (directory_iterator == NULL) {
        return NULL;
    }
    while ((entry = readdir(directory_iterator)) != NULL) {
        if (entry->d_type == DT_REG) {
            return entry->d_name;
        }
    }
    closedir(directory_iterator);
    directory_iterator = NULL;
    return NULL;
}

FILE* OS_fopen(const char* pathname, const char* mode) {
    FILE* f = fopen(pathname, mode);
    if (f != NULL) {
        return f;
    }
    char buffer[512];
    char buffer2[512];
    strcpy(buffer, pathname);
    strcpy(buffer2, pathname);
    char* pDirName = dirname(buffer);
    char* pBaseName = basename(buffer2);
    DIR* pDir = opendir(pDirName);
    if (pDir == NULL) {
        return NULL;
    }
    for (struct dirent* pDirent = readdir(pDir); pDirent != NULL; pDirent = readdir(pDir)) {
        if (strcasecmp(pBaseName, pDirent->d_name) == 0) {
            strcat(pDirName, "/");
            strcat(pDirName, pDirent->d_name);
            f = fopen(pDirName, mode);
            break;
        }
    }
    closedir(pDir);
    if (harness_game_config.verbose) {
        if (f == NULL) {
            fprintf(stderr, "Failed to open \"%s\" (%s)\n", pathname, strerror(errno));
        }
    }
    return f;
}

size_t OS_ConsoleReadPassword(char* pBuffer, size_t pBufferLen) {
    struct termios old, new;
    char c;
    size_t len;

    tcgetattr(STDIN_FILENO, &old);
    new = old;
    new.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &new);

    len = 0;
    pBuffer[len] = '\0';
    while (1) {
        if (read(STDIN_FILENO, &c, 1) == 1) {
            if (c == 0x7f) {
                if (len > 0) {
                    len--;
                    pBuffer[len] = '\0';
                    printf("\033[1D \033[1D");
                    fflush(stdout);
                    continue;
                }
            } else if (c == '\r' || c == '\n') {
                printf("\n");
                fflush(stdout);
                break;
            } else if (len < pBufferLen - 1) {
                if ((c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z')) {
                    pBuffer[len] = c;
                    printf("*");
                    fflush(stdout);
                    len++;
                    pBuffer[len] = '\0';
                }
            }
        }
    }

    tcsetattr(STDIN_FILENO, TCSANOW, &old);
    return len;
}

char* OS_Dirname(const char* path) {
    strcpy(name_buf, path);
    return dirname(name_buf);
}

char* OS_Basename(const char* path) {
    strcpy(name_buf, path);
    return basename(name_buf);
}

char* OS_GetWorkingDirectory(char* argv0) {
    return OS_Dirname(argv0);
}

int OS_GetPrefPath(char* dest, char* app) {
    const char* base = NULL;
    char path[1024];
    struct stat statbuf;

    base = getenv("XDG_DATA_HOME");
    if (!base) {
        const char* home = getenv("HOME");
        if (home == NULL) {
            return -1;
        }
        if (snprintf(path, sizeof(path), "%s/.local/share", home) >= (int)sizeof(path)) {
            return -1;
        }
        mkdir(path, 0755);
        base = path;
    }

    if (stat(base, &statbuf) == -1) {
        return -1;
    }
    if (!(statbuf.st_mode & S_IFDIR)) {
        return -1;
    }

    if (snprintf(name_buf, sizeof(name_buf), "%s/%s/", base, app)  >= (int)sizeof(name_buf)) {
        return -1;
    }
    mkdir(name_buf, 0755);

    strcpy(dest, name_buf);
    return 0;
}

int OS_GetAdapterAddress(char* name, void* pSockaddr_in) {
    struct ifaddrs *ifaddr, *ifa;
    int found = 0;

    // Get the list of network interfaces
    if (getifaddrs(&ifaddr) == -1) {
        perror("getifaddrs");
        return 0;
    }

    // Loop through all interfaces
    for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr == NULL)
            continue;

        if (strlen(name) == 0 || strcmp(name, ifa->ifa_name) == 0) {
            if (ifa->ifa_addr->sa_family == AF_INET) {
                *((struct sockaddr_in*)pSockaddr_in) = *(struct sockaddr_in*)ifa->ifa_addr;
                found = 1;
                break;
            }
        }
    }

    // Free the memory allocated by getifaddrs
    freeifaddrs(ifaddr);
    return found;
}

int OS_InitSockets(void) {
    return 0;
}

int OS_GetLastSocketError(void) {
    return errno;
}

void OS_CleanupSockets(void) {
    // no-op
}

int OS_SetSocketNonBlocking(int socket) {
    int flags = fcntl(socket, F_GETFL);
    flags |= O_NONBLOCK;
    return fcntl(socket, F_SETFL, flags);
}

int OS_CloseSocket(int socket) {
    return close(socket);
}
