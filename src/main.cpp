#include <getopt.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <fstream>

#include "ev.h"
#include "yaml.h"

#include "lwip/init.h"
#include "lwip/mem.h"
#include "lwip/timeouts.h"
#include "lwip/ip.h"
#include "lwip/ip4_frag.h"

#include "struct.h"
#include "util.h"
#include "var.h"

#if defined(LWIP_UNIX_LINUX)

#include "netif/tapif.h"
#include "netif/etharp.h"

#endif

#include "netif/tunif.h"

#include "udp_raw.h"
#include "tcp_raw.h"

/* lwip host IP configuration */
struct netif netif;
static ip4_addr_t ipaddr, netmask, gw;
static char *config_file;

/* nonstatic debug cmd option, exported in lwipopts.h */
unsigned char debug_flags;

static struct option longopts[] = {
        /* turn on debugging output (if build with LWIP_DEBUG) */
        {"debug",  no_argument,       NULL, 'd'},
        /* help */
        {"help",   no_argument,       NULL, 'h'},
        /* config file */
        {"config", required_argument, NULL, 'c'},
        /* new command line options go here! */
        {NULL, 0,                     NULL, 0}
};


void on_shell();

void down_shell();

void tuntap_read_cb(struct ev_loop *loop, struct ev_io *watcher, int revents);

void sigterm_cb(struct ev_loop *loop, ev_signal *watcher, int revents);

void sigint_cb(struct ev_loop *loop, ev_signal *watcher, int revents);

void sigusr2_cb(struct ev_loop *loop, ev_signal *watcher, int revents);

static void
usage(void) {
    unsigned char i;
    printf("options:\n");
    for (i = 0; i < NUM_OPTS; i++) {
        printf("-%c --%s\n", longopts[i].val, longopts[i].name);
    }
}

void parse_config(int argc, char **argv) {
    int ch;
    char ip_str[16] = {0}, nm_str[16] = {0}, gw_str[16] = {0};

    /* startup defaults (may be overridden by one or more opts) */
    IP4_ADDR(&gw, 10, 0, 0, 1);
    // ip4_addr_set_any(&gw);
    ip4_addr_set_any(&ipaddr);
    IP4_ADDR(&ipaddr, 10, 0, 0, 2);
    IP4_ADDR(&netmask, 255, 255, 255, 0);

    /* use debug flags defined by debug.h */
    debug_flags = LWIP_DBG_OFF;

    while ((ch = getopt_long(argc, argv, "dhc:", longopts, NULL)) != -1) {
        switch (ch) {
            case 'd':
                debug_flags |= (LWIP_DBG_ON | LWIP_DBG_TRACE | LWIP_DBG_STATE | LWIP_DBG_FRESH | LWIP_DBG_HALT);
                break;
            case 'h':
                usage();
                exit(0);
            case 'c':
                config_file = optarg;
                break;
            default:
                usage();
                break;
        }
    }
    argc -= optind;
    argv += optind;

    if (config_file == NULL) {
        printf("Please provide config file\n");
        exit(0);
    }

    /**
     * yaml config parser start
     */
    FILE *fh = fopen(config_file, "r");
    yaml_parser_t parser;
    yaml_token_t token;   /* new variable */

    /* Initialize parser */
    if (!yaml_parser_initialize(&parser))
        fputs("Failed to initialize parser!\n", stderr);
    if (fh == NULL)
        fputs("Failed to open file!\n", stderr);

    /* Set input file */
    yaml_parser_set_input_file(&parser, fh);

    /**
     * state = 0 = expect key
     * state = 1 = expect value
     */
    int state = 0;
    char **datap;
    char *tk;

    /* BEGIN new code */
    do {
        yaml_parser_scan(&parser, &token);
        switch (token.type) {
            /* Stream start/end */
            case YAML_STREAM_START_TOKEN:
                break;
            case YAML_STREAM_END_TOKEN:
                break;
                /* Token types (read before actual token) */
            case YAML_KEY_TOKEN:
                state = 0;
                break;
            case YAML_VALUE_TOKEN:
                state = 1;
                break;
                /* Block delimeters */
            case YAML_BLOCK_SEQUENCE_START_TOKEN:
                break;
            case YAML_BLOCK_ENTRY_TOKEN:
                break;
            case YAML_BLOCK_END_TOKEN:
                break;
                /* Data */
            case YAML_BLOCK_MAPPING_START_TOKEN:
                break;
            case YAML_SCALAR_TOKEN:
                tk = (char *) token.data.scalar.value;
                if (state == 0) {
                    if (strcmp(tk, "ip_mode") == 0) {
                        datap = &conf->ip_mode;
                    } else if (strcmp(tk, "dns_mode") == 0) {
                        datap = &conf->dns_mode;
                    } else if (strcmp(tk, "socks_server") == 0) {
                        datap = &conf->socks_server;
                    } else if (strcmp(tk, "socks_port") == 0) {
                        datap = &conf->socks_port;
                    } else if (strcmp(tk, "remote_dns_server") == 0) {
                        datap = &conf->remote_dns_server;
                    } else if (strcmp(tk, "remote_dns_port") == 0) {
                        datap = &conf->remote_dns_port;
                    } else if (strcmp(tk, "local_dns_port") == 0) {
                        datap = &conf->local_dns_port;
                    } else if (strcmp(tk, "relay_none_dns_packet_with_udp") == 0) {
                        datap = &conf->relay_none_dns_packet_with_udp;
                    } else if (strcmp(tk, "custom_domian_server_file") == 0) {
                        datap = &conf->custom_domian_server_file;
                    } else if (strcmp(tk, "gw") == 0) {
                        datap = &conf->gw;
                    } else if (strcmp(tk, "addr") == 0) {
                        datap = &conf->addr;
                    } else if (strcmp(tk, "netmask") == 0) {
                        datap = &conf->netmask;
                    } else if (strcmp(tk, "after_start_shell") == 0) {
                        datap = &conf->after_start_shell;
                    } else if (strcmp(tk, "before_shutdown_shell") == 0) {
                        datap = &conf->before_shutdown_shell;
                    } else {
                        printf("Unrecognised key: %s\n", tk);
                    }
                } else {
                    *datap = strdup(tk);
                }
                break;
                /* Others */
            default:
                break;
        }
        if (token.type != YAML_STREAM_END_TOKEN)
            yaml_token_delete(&token);
    } while (token.type != YAML_STREAM_END_TOKEN);
    yaml_token_delete(&token);
    /* END new code */

    /* Cleanup */
    yaml_parser_delete(&parser);
    fclose(fh);
    /**
     * yaml config parser end
     */

    /**
     * if config, overside default value
     */
    if (conf->gw != NULL) {
        ip4addr_aton(conf->gw, &gw);
    }
    if (conf->addr != NULL) {
        ip4addr_aton(conf->addr, &ipaddr);
    }
    if (conf->netmask != NULL) {
        ip4addr_aton(conf->netmask, &netmask);
    }

    if (conf->ip_mode == NULL) {
        memcpy(conf->ip_mode, "tun", 3);
    } else {
#if defined(LWIP_UNIX_MACH)
        if (strcmp("tap", conf->ip_mode) == 0) {
          printf("Darwin does not support tap mode!!!\n");
          exit(0);
        }
#endif /* LWIP_UNIX_MACH */
    }
    if (conf->dns_mode == NULL) {
        memcpy(conf->dns_mode, "tcp", 3);
    }

    strncpy(ip_str, ip4addr_ntoa(&ipaddr), sizeof(ip_str));
    strncpy(nm_str, ip4addr_ntoa(&netmask), sizeof(nm_str));
    strncpy(gw_str, ip4addr_ntoa(&gw), sizeof(gw_str));
    printf("Lwip netstack host %s mask %s gateway %s\n", ip_str, nm_str, gw_str);


    if (conf->custom_domian_server_file != NULL) {
        std::string line;
        std::string file_sp(";");
        std::string sp(SLASH);

        std::vector<std::vector<std::string>> domains;

        std::vector<std::string> files;
        std::string ffs(conf->custom_domian_server_file);
        split(ffs, file_sp, &files);
        for (int i = 0; i < files.size(); ++i) {
            if (!files.at(i).empty()) {
                std::ifstream chndomains(files.at(i));
                if (chndomains.is_open()) {
                    while (getline(chndomains, line)) {
                        if (!line.empty() && line.substr(0, 1) != "#") {
                            std::vector<std::string> v;
                            split(line, sp, &v);
                            domains.push_back(v);
                        }
                    }
                    chndomains.close();
                } else {
                    std::cout << "Unable to open dns domain file " << files.at(i) << std::endl;
                }
            }
        }
        conf->domains = domains;
    }
}

int
main(int argc, char **argv) {
    parse_config(argc, argv);
    /* lwip/src/core/init.c */
    lwip_init();

    if (strcmp(conf->ip_mode, "tun") == 0) {
        netif_add(&netif, &ipaddr, &netmask, &gw, NULL, tunif_init, ip_input); // IPV4 IPV6 TODO
    } else {
#if defined(LWIP_UNIX_LINUX)
        netif_add(&netif, &ipaddr, &netmask, &gw, NULL, tapif_init, ethernet_input);
#endif
    }
    netif_set_default(&netif);
    netif_set_link_up(&netif);
    /* lwip/core/netif.c */
    netif_set_up(&netif);
#if LWIP_IPV6
    netif_create_ip6_linklocal_address(&netif, 1);
#endif

    udp_raw_init();
    tcp_raw_init();

    struct ev_io *tuntap_io = (struct ev_io *) mem_malloc(sizeof(struct ev_io));
    if (tuntap_io == NULL) {
        printf("tuntap_io: out of memory for tuntap_io\n");
        return -1;
    }
    struct ev_loop *loop = ev_default_loop(0);

    struct tuntapif *tuntapif;
    tuntapif = (struct tuntapif *) ((&netif)->state);

    /**
     * signal start
     */
    // eg: kill
    ev_signal signal_term_watcher;
    ev_signal_init(&signal_term_watcher, sigterm_cb, SIGTERM);
    ev_signal_start(loop, &signal_term_watcher);

    // eg: ctrl + c
    ev_signal signal_int_watcher;
    ev_signal_init(&signal_int_watcher, sigint_cb, SIGINT);
    ev_signal_start(loop, &signal_int_watcher);

    ev_signal signal_usr2_watcher;
    ev_signal_init(&signal_usr2_watcher, sigusr2_cb, SIGUSR2);
    ev_signal_start(loop, &signal_usr2_watcher);
    /**
     * signal end
     */

    ev_io_init(tuntap_io, tuntap_read_cb, tuntapif->fd, EV_READ);
    ev_io_start(loop, tuntap_io);


    // TODO
    sys_check_timeouts();

    /**
     * setup shell scripts
     */
    on_shell();

    std::cout << "Ip2socks started!" << std::endl;
    return ev_run(loop, 0);
}

/**
 * down shell scripts
 */
void down_shell() {
    if (conf->before_shutdown_shell != NULL) {
        std::string sh("sh ");
        sh.append(conf->before_shutdown_shell);
        sh.append(" ");

        if (strcmp(conf->ip_mode, "tun") == 0) {
            sh.append(conf->gw);
        } else {
            sh.append(conf->addr);
        }

        printf("exec down shell %s\n", sh.c_str());

        system(sh.c_str());
    }
}

/**
 * setup shell scripts
 */
void on_shell() {
    // todo: this has a problem to init
    if (conf->after_start_shell != NULL) {
        std::string sh("sh ");
        sh.append(conf->after_start_shell);
        sh.append(" ");

        if (strcmp(conf->ip_mode, "tun") == 0) {
            sh.append(conf->gw);
        } else {
            sh.append(conf->addr);
        }

        printf("exec setup shell %s\n", sh.c_str());

        system(sh.c_str());
    }
}

void sigterm_cb(struct ev_loop *loop, ev_signal *watcher, int revents) {
    printf("SIGTERM handler called in process!!!\n");
    down_shell();
    ev_break(loop, EVBREAK_ALL);
    exit(0); // kill all threads
}

void sigint_cb(struct ev_loop *loop, ev_signal *watcher, int revents) {
    printf("SIGINT handler called in process!!!\n");
    down_shell();
    ev_break(loop, EVBREAK_ALL);
    exit(0); // kill all threads
}

void sigusr2_cb(struct ev_loop *loop, ev_signal *watcher, int revents) {
    printf("SIGUSR2 handler called in process!!! TODO reload config.\n");
}

void tuntap_read_cb(struct ev_loop *loop, struct ev_io *watcher, int revents) {
    if (strcmp(conf->ip_mode, "tun") == 0) {
        tunif_input(&netif);
    } else {
#if defined(LWIP_UNIX_LINUX)
        tapif_input(&netif);
#endif
    }
}
