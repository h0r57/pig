/*
 *                                Copyright (C) 2015 by Rafael Santiago
 *
 * This is a free software. You can redistribute it and/or modify under
 * the terms of the GNU General Public License version 2.
 *
 */
#include "types.h"
#include "pigsty.h"
#include "lists.h"
#include "oink.h"
#include <stdio.h>
#include <string.h>
#include <signal.h>

static int should_exit = 0;

static int should_be_quiet = 0;

static char *get_option(const char *option, char *default_value, const int argc, char **argv);

static void sigint_watchdog(int signr);

static pigsty_entry_ctx *load_signatures(const char *signatures);

static void run_pig_run(const char *signatures, const char *iface, const char *timeout);

static char *get_option(const char *option, char *default_value, const int argc, char **argv) {
    static char retval[8192];
    int a;
    char temp[8192] = "";
    memset(temp, 0, sizeof(temp));
    temp[0] = '-';
    temp[1] = '-';
    strncpy(&temp[2], option, sizeof(temp) - 1);
    for (a = 0; a < argc; a++) {
        if (strcmp(argv[a], temp) == 0) {
            return "1";
        }
    }
    strcat(temp, "=");
    for (a = 0; a < argc; a++) {
        if (strstr(argv[a], temp) == argv[a]) {
            return argv[a] + strlen(temp);
        }
    }
    memset(retval, 0, sizeof(retval));
    if (default_value != NULL) {
        strncpy(retval, default_value, sizeof(retval) - 1);
    } else {
        return NULL;
    }
    return retval;
}

static void sigint_watchdog(int signr) {
    if (!should_be_quiet) {
        printf("\npig INFO: exiting... please wait...\n");
    }
    should_exit = 1;
}

static pigsty_entry_ctx *load_signatures(const char *signatures) {
    pigsty_entry_ctx *sig_entries = NULL;
    const char *sp = NULL;
    char curr_file_path[8192] = "";
    char *cfp = NULL;
    sp = signatures;
    cfp = &curr_file_path[0];
    while (*sp != 0) {
        if (*sp != ',' && *(sp + 1) != 0) {
            *cfp = *sp;
            cfp++;
        } else {
            if (*(sp + 1) == 0) {
                if (*sp != ',') {
                    *cfp = *sp;
                    cfp++;
                }
            }
            *cfp = '\0';
            if (!should_be_quiet) {
                printf("pig INFO: loading \"%s\"...\n", curr_file_path);
            }
            sig_entries = load_pigsty_data_from_file(sig_entries, curr_file_path);
            if (sig_entries == NULL) {
                if (!should_be_quiet) {
                    printf("pig INFO: load failure.\n");
                }
                break;
            }
            if (!should_be_quiet) {
                printf("pig INFO: load success.\n");
            }
            cfp = &curr_file_path[0];
        }
        sp++;
    }
    return sig_entries;
}

static void run_pig_run(const char *signatures, const char *iface, const char *timeout) {
    int timeo = 10;
    pigsty_entry_ctx *pigsty = NULL;
    size_t signatures_count = 0;
    pigsty_entry_ctx *signature = NULL;
    int sockfd = -1;
    if (timeout != NULL) {
        timeo = atoi(timeout);
    }
    if (!should_be_quiet) {
        printf("pig INFO: starting up pig engine...\n\n");
    }
    sockfd = init_raw_socket();
    if (sockfd == -1) {
        printf("pig PANIC: unable to create the socket.\npig ERROR: aborted.\n");
        return;
    }
    pigsty = load_signatures(signatures);
    if (pigsty == NULL) {
        printf("pig ERROR: aborted.\n");
        deinit_raw_socket(sockfd);
        return;
    }
    signatures_count = get_pigsty_entry_count(pigsty);
    if (!should_be_quiet) {
        printf("\npig INFO: done (%d signature(s) read).\n\n", signatures_count);
    }
    while (!should_exit) {
        signature = get_pigsty_entry_by_index(rand() % signatures_count, pigsty);
        if (signature == NULL) {
            continue; //  WARN(Santiago): It should never happen. However... Sometimes... The World tends to be a rather weird place.
        }
        if (oink(signature, sockfd)) {
            //  TODO(Santiago): Send it.
            if (!should_be_quiet) {
                printf("pig INFO: a packet based on signature was sent.\n");
            }
            sleep(timeo);
        }
    }
    del_pigsty_entry(pigsty);
    deinit_raw_socket(sockfd);
}

int main(int argc, char **argv) {
    char *signatures = NULL;
    char *iface = NULL;
    char *timeout = NULL;
    char *tp = NULL;
    if (get_option("version", NULL, argc, argv) != NULL) {
        printf("pig v%s\n", PIG_VERSION);
        return 0;
    }
    if (argc > 2) {
        signatures = get_option("signatures", NULL, argc, argv);
        if (signatures == NULL) {
            printf("pig ERROR: --signatures option is missing.\n");
            return 1;
        }
        iface = get_option("iface", NULL, argc, argv);
        if (iface == NULL) {
            printf("pig ERROR: --iface option is missing.\n");
            return 1;
        }
        timeout = get_option("timeout", NULL, argc, argv);
        if (timeout != NULL) {
            for (tp = timeout; *tp != 0; tp++) {
                if (!isdigit(*tp)) {
                    printf("pig ERROR: an invalid timeout value was supplied.\n");
                    return 1;
                }
            }
        }
        should_be_quiet = (get_option("no-echo", NULL, argc, argv) != NULL);
        signal(SIGINT, sigint_watchdog);
        signal(SIGTERM, sigint_watchdog);
        srand(time(0));
        run_pig_run(signatures, iface, timeout);
    } else {
        printf("usage: %s --signatures=file.0,file.1,(...),file.n --iface=<nic> [--timeout=<in secs> --no-echo]\n", argv[0]);
    }
    return 0;
}
