#include "nta_cli.h"

#include <getopt.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define NTA_PARSE_HELP 2

typedef enum {
    NTA_OUTPUT_TEXT = 0,
    NTA_OUTPUT_JSON,
    NTA_OUTPUT_CSV,
} nta_output_format;

typedef struct {
    nta_output_format output;
} nta_overview_options;

typedef struct {
    nta_output_format output;
    const char *pid;
    const char *interface;
    bool listening;
    bool verbose;
} nta_connections_options;

typedef struct {
    nta_output_format output;
    const char *name;
    bool up_only;
    bool verbose;
} nta_interfaces_options;

typedef struct {
    nta_output_format output;
    const char *table;
    const char *interface;
    bool verbose;
} nta_routes_options;

typedef struct {
    nta_output_format output;
    bool verbose;
} nta_dns_options;

typedef struct {
    nta_output_format output;
    const char *target;
    int count;
    bool verbose;
} nta_ping_options;

static const struct option NTA_OVERVIEW_OPTIONS[] = {
    {"json", no_argument, NULL, 'j'},
    {"csv", no_argument, NULL, 'c'},
    {"text", no_argument, NULL, 'x'},
    {"help", no_argument, NULL, 'h'},
    {NULL, 0, NULL, 0},
};

static const struct option NTA_CONNECTIONS_OPTIONS[] = {
    {"pid", required_argument, NULL, 'p'},
    {"interface", required_argument, NULL, 'i'},
    {"listening", no_argument, NULL, 'l'},
    {"json", no_argument, NULL, 'j'},
    {"csv", no_argument, NULL, 'c'},
    {"text", no_argument, NULL, 'x'},
    {"verbose", no_argument, NULL, 'v'},
    {"help", no_argument, NULL, 'h'},
    {NULL, 0, NULL, 0},
};

static const struct option NTA_INTERFACES_OPTIONS[] = {
    {"name", required_argument, NULL, 'i'},
    {"up", no_argument, NULL, 'u'},
    {"json", no_argument, NULL, 'j'},
    {"csv", no_argument, NULL, 'c'},
    {"text", no_argument, NULL, 'x'},
    {"verbose", no_argument, NULL, 'v'},
    {"help", no_argument, NULL, 'h'},
    {NULL, 0, NULL, 0},
};

static const struct option NTA_ROUTES_OPTIONS[] = {
    {"table", required_argument, NULL, 'T'},
    {"interface", required_argument, NULL, 'i'},
    {"json", no_argument, NULL, 'j'},
    {"csv", no_argument, NULL, 'c'},
    {"text", no_argument, NULL, 'x'},
    {"verbose", no_argument, NULL, 'v'},
    {"help", no_argument, NULL, 'h'},
    {NULL, 0, NULL, 0},
};

static const struct option NTA_DNS_OPTIONS[] = {
    {"json", no_argument, NULL, 'j'},
    {"csv", no_argument, NULL, 'c'},
    {"text", no_argument, NULL, 'x'},
    {"verbose", no_argument, NULL, 'v'},
    {"help", no_argument, NULL, 'h'},
    {NULL, 0, NULL, 0},
};

static const struct option NTA_PING_OPTIONS[] = {
    {"target", required_argument, NULL, 't'},
    {"count", required_argument, NULL, 'n'},
    {"json", no_argument, NULL, 'j'},
    {"csv", no_argument, NULL, 'c'},
    {"text", no_argument, NULL, 'x'},
    {"verbose", no_argument, NULL, 'v'},
    {"help", no_argument, NULL, 'h'},
    {NULL, 0, NULL, 0},
};

static const char *nta_output_name(nta_output_format output)
{
    switch (output) {
    case NTA_OUTPUT_TEXT:
        return "text";
    case NTA_OUTPUT_JSON:
        return "json";
    case NTA_OUTPUT_CSV:
        return "csv";
    }

    return "unknown";
}

static void nta_set_output(nta_output_format *output, int option)
{
    if (option == 'j') {
        *output = NTA_OUTPUT_JSON;
    } else if (option == 'c') {
        *output = NTA_OUTPUT_CSV;
    } else if (option == 'x') {
        *output = NTA_OUTPUT_TEXT;
    }
}

static void nta_print_usage(FILE *stream, const char *program)
{
    fprintf(stream, "Usage: %s <command> [options]\n\n", program);
    fprintf(stream, "Commands:\n");
    fprintf(stream, "  overview      Show a high-level network summary\n");
    fprintf(stream, "  connections   List active network connections\n");
    fprintf(stream, "  interfaces    List network interfaces\n");
    fprintf(stream, "  routes        Show routing information\n");
    fprintf(stream, "  dns           Show DNS resolver information\n");
    fprintf(stream, "  ping          Run a basic reachability diagnostic\n\n");
    fprintf(stream, "Run '%s <command> --help' for command-specific options.\n", program);
}

static void nta_print_command_usage(FILE *stream, const char *program, const char *command)
{
    if (strcmp(command, "overview") == 0) {
        fprintf(stream, "Usage: %s overview [--json|--csv|--text]\n", program);
    } else if (strcmp(command, "connections") == 0) {
        fprintf(stream, "Usage: %s connections [options] [--json|--csv|--text]\n\n", program);
        fprintf(stream, "Options:\n");
        fprintf(stream, "  -p, --pid <pid>          Filter by process ID\n");
        fprintf(stream, "  -i, --interface <name>   Filter by interface name\n");
        fprintf(stream, "  -l, --listening          Show listening sockets only\n");
        fprintf(stream, "  -v, --verbose            Print more detail\n");
    } else if (strcmp(command, "interfaces") == 0) {
        fprintf(stream, "Usage: %s interfaces [options] [--json|--csv|--text]\n\n", program);
        fprintf(stream, "Options:\n");
        fprintf(stream, "  -i, --name <name>        Show one interface\n");
        fprintf(stream, "  -u, --up                 Show interfaces that are up only\n");
        fprintf(stream, "  -v, --verbose            Print more detail\n");
    } else if (strcmp(command, "routes") == 0) {
        fprintf(stream, "Usage: %s routes [options] [--json|--csv|--text]\n\n", program);
        fprintf(stream, "Options:\n");
        fprintf(stream, "  -T, --table <table>      Route table name or ID\n");
        fprintf(stream, "  -i, --interface <name>   Filter by interface name\n");
        fprintf(stream, "  -v, --verbose            Print more detail\n");
    } else if (strcmp(command, "dns") == 0) {
        fprintf(stream, "Usage: %s dns [--verbose] [--json|--csv|--text]\n", program);
    } else if (strcmp(command, "ping") == 0) {
        fprintf(stream, "Usage: %s ping --target <host> [options] [--json|--csv|--text]\n\n", program);
        fprintf(stream, "Options:\n");
        fprintf(stream, "  -t, --target <host>      Target host or IP address\n");
        fprintf(stream, "  -n, --count <count>      Number of probes\n");
        fprintf(stream, "  -v, --verbose            Print more detail\n");
    } else {
        nta_print_usage(stream, program);
    }
}

static bool nta_is_command(const char *command)
{
    return strcmp(command, "overview") == 0 ||
           strcmp(command, "connections") == 0 ||
           strcmp(command, "interfaces") == 0 ||
           strcmp(command, "routes") == 0 ||
           strcmp(command, "dns") == 0 ||
           strcmp(command, "ping") == 0;
}

static int nta_reject_extra_args(int argc, char **argv, const char *command)
{
    if (optind < argc) {
        fprintf(stderr, "netra: unexpected argument '%s'\n\n", argv[optind]);
        nta_print_command_usage(stderr, argv[0], command);
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

static int nta_parse_count(const char *value, int *count)
{
    char *end = NULL;
    const long parsed = strtol(value, &end, 10);

    if (*value == '\0' || *end != '\0' || parsed <= 0 || parsed > 1000) {
        fprintf(stderr, "netra: count must be a number between 1 and 1000\n");
        return EXIT_FAILURE;
    }

    *count = (int)parsed;
    return EXIT_SUCCESS;
}

static void nta_print_json_string(const char *value)
{
    if (value == NULL) {
        printf("null");
        return;
    }

    putchar('"');
    for (const char *cursor = value; *cursor != '\0'; ++cursor) {
        switch (*cursor) {
        case '\\':
            printf("\\\\");
            break;
        case '"':
            printf("\\\"");
            break;
        case '\n':
            printf("\\n");
            break;
        case '\r':
            printf("\\r");
            break;
        case '\t':
            printf("\\t");
            break;
        default:
            putchar(*cursor);
            break;
        }
    }
    putchar('"');
}

static void nta_print_stub_text(const char *command, nta_output_format output)
{
    printf("netra %s\n", command);
    printf("  output: %s\n", nta_output_name(output));
    printf("\nCommand parsing is ready; diagnostic implementation is pending.\n");
}

static int nta_print_overview(const nta_overview_options *opts)
{
    if (opts->output == NTA_OUTPUT_JSON) {
        printf("{\n  \"command\": \"overview\",\n  \"format\": \"json\",\n  \"status\": \"stub\"\n}\n");
    } else if (opts->output == NTA_OUTPUT_CSV) {
        printf("command,format,status\n");
        printf("overview,csv,stub\n");
    } else {
        nta_print_stub_text("overview", opts->output);
    }

    return EXIT_SUCCESS;
}

static int nta_print_connections(const nta_connections_options *opts)
{
    if (opts->output == NTA_OUTPUT_JSON) {
        printf("{\n  \"command\": \"connections\",\n  \"format\": \"json\",\n");
        printf("  \"pid\": ");
        nta_print_json_string(opts->pid);
        printf(",\n  \"interface\": ");
        nta_print_json_string(opts->interface);
        printf(",\n  \"listening\": %s,\n", opts->listening ? "true" : "false");
        printf("  \"verbose\": %s,\n  \"status\": \"stub\"\n}\n", opts->verbose ? "true" : "false");
    } else if (opts->output == NTA_OUTPUT_CSV) {
        printf("command,format,pid,interface,listening,verbose,status\n");
        printf("connections,csv,%s,%s,%s,%s,stub\n",
               opts->pid != NULL ? opts->pid : "",
               opts->interface != NULL ? opts->interface : "",
               opts->listening ? "true" : "false",
               opts->verbose ? "true" : "false");
    } else {
        nta_print_stub_text("connections", opts->output);
        printf("  pid:       %s\n", opts->pid != NULL ? opts->pid : "-");
        printf("  interface: %s\n", opts->interface != NULL ? opts->interface : "-");
        printf("  listening: %s\n", opts->listening ? "yes" : "no");
        printf("  verbose:   %s\n", opts->verbose ? "yes" : "no");
    }

    return EXIT_SUCCESS;
}

static int nta_print_interfaces(const nta_interfaces_options *opts)
{
    if (opts->output == NTA_OUTPUT_JSON) {
        printf("{\n  \"command\": \"interfaces\",\n  \"format\": \"json\",\n");
        printf("  \"name\": ");
        nta_print_json_string(opts->name);
        printf(",\n  \"up_only\": %s,\n", opts->up_only ? "true" : "false");
        printf("  \"verbose\": %s,\n  \"status\": \"stub\"\n}\n", opts->verbose ? "true" : "false");
    } else if (opts->output == NTA_OUTPUT_CSV) {
        printf("command,format,name,up_only,verbose,status\n");
        printf("interfaces,csv,%s,%s,%s,stub\n",
               opts->name != NULL ? opts->name : "",
               opts->up_only ? "true" : "false",
               opts->verbose ? "true" : "false");
    } else {
        nta_print_stub_text("interfaces", opts->output);
        printf("  name:    %s\n", opts->name != NULL ? opts->name : "-");
        printf("  up only: %s\n", opts->up_only ? "yes" : "no");
        printf("  verbose: %s\n", opts->verbose ? "yes" : "no");
    }

    return EXIT_SUCCESS;
}

static int nta_print_routes(const nta_routes_options *opts)
{
    if (opts->output == NTA_OUTPUT_JSON) {
        printf("{\n  \"command\": \"routes\",\n  \"format\": \"json\",\n");
        printf("  \"table\": ");
        nta_print_json_string(opts->table);
        printf(",\n  \"interface\": ");
        nta_print_json_string(opts->interface);
        printf(",\n  \"verbose\": %s,\n  \"status\": \"stub\"\n}\n", opts->verbose ? "true" : "false");
    } else if (opts->output == NTA_OUTPUT_CSV) {
        printf("command,format,table,interface,verbose,status\n");
        printf("routes,csv,%s,%s,%s,stub\n",
               opts->table != NULL ? opts->table : "",
               opts->interface != NULL ? opts->interface : "",
               opts->verbose ? "true" : "false");
    } else {
        nta_print_stub_text("routes", opts->output);
        printf("  table:     %s\n", opts->table != NULL ? opts->table : "-");
        printf("  interface: %s\n", opts->interface != NULL ? opts->interface : "-");
        printf("  verbose:   %s\n", opts->verbose ? "yes" : "no");
    }

    return EXIT_SUCCESS;
}

static int nta_print_dns(const nta_dns_options *opts)
{
    if (opts->output == NTA_OUTPUT_JSON) {
        printf("{\n  \"command\": \"dns\",\n  \"format\": \"json\",\n");
        printf("  \"verbose\": %s,\n  \"status\": \"stub\"\n}\n", opts->verbose ? "true" : "false");
    } else if (opts->output == NTA_OUTPUT_CSV) {
        printf("command,format,verbose,status\n");
        printf("dns,csv,%s,stub\n", opts->verbose ? "true" : "false");
    } else {
        nta_print_stub_text("dns", opts->output);
        printf("  verbose: %s\n", opts->verbose ? "yes" : "no");
    }

    return EXIT_SUCCESS;
}

static int nta_print_ping(const nta_ping_options *opts)
{
    if (opts->output == NTA_OUTPUT_JSON) {
        printf("{\n  \"command\": \"ping\",\n  \"format\": \"json\",\n");
        printf("  \"target\": ");
        nta_print_json_string(opts->target);
        printf(",\n  \"count\": %d,\n", opts->count);
        printf("  \"verbose\": %s,\n  \"status\": \"stub\"\n}\n", opts->verbose ? "true" : "false");
    } else if (opts->output == NTA_OUTPUT_CSV) {
        printf("command,format,target,count,verbose,status\n");
        printf("ping,csv,%s,%d,%s,stub\n",
               opts->target != NULL ? opts->target : "",
               opts->count,
               opts->verbose ? "true" : "false");
    } else {
        nta_print_stub_text("ping", opts->output);
        printf("  target:  %s\n", opts->target != NULL ? opts->target : "-");
        printf("  count:   %d\n", opts->count);
        printf("  verbose: %s\n", opts->verbose ? "yes" : "no");
    }

    return EXIT_SUCCESS;
}

static int nta_run_overview(int argc, char **argv)
{
    nta_overview_options opts = {.output = NTA_OUTPUT_TEXT};

    optind = 2;
    while (true) {
        const int option = getopt_long(argc, argv, "h", NTA_OVERVIEW_OPTIONS, NULL);
        if (option == -1) {
            break;
        }

        if (option == 'j' || option == 'c' || option == 'x') {
            nta_set_output(&opts.output, option);
        } else if (option == 'h') {
            nta_print_command_usage(stdout, argv[0], "overview");
            return NTA_PARSE_HELP;
        } else {
            nta_print_command_usage(stderr, argv[0], "overview");
            return EXIT_FAILURE;
        }
    }

    if (nta_reject_extra_args(argc, argv, "overview") != EXIT_SUCCESS) {
        return EXIT_FAILURE;
    }

    return nta_print_overview(&opts);
}

static int nta_run_connections(int argc, char **argv)
{
    nta_connections_options opts = {
        .output = NTA_OUTPUT_TEXT,
        .pid = NULL,
        .interface = NULL,
        .listening = false,
        .verbose = false,
    };

    optind = 2;
    while (true) {
        const int option = getopt_long(argc, argv, "p:i:lvh", NTA_CONNECTIONS_OPTIONS, NULL);
        if (option == -1) {
            break;
        }

        switch (option) {
        case 'p':
            opts.pid = optarg;
            break;
        case 'i':
            opts.interface = optarg;
            break;
        case 'l':
            opts.listening = true;
            break;
        case 'v':
            opts.verbose = true;
            break;
        case 'j':
        case 'c':
        case 'x':
            nta_set_output(&opts.output, option);
            break;
        case 'h':
            nta_print_command_usage(stdout, argv[0], "connections");
            return NTA_PARSE_HELP;
        default:
            nta_print_command_usage(stderr, argv[0], "connections");
            return EXIT_FAILURE;
        }
    }

    if (nta_reject_extra_args(argc, argv, "connections") != EXIT_SUCCESS) {
        return EXIT_FAILURE;
    }

    return nta_print_connections(&opts);
}

static int nta_run_interfaces(int argc, char **argv)
{
    nta_interfaces_options opts = {
        .output = NTA_OUTPUT_TEXT,
        .name = NULL,
        .up_only = false,
        .verbose = false,
    };

    optind = 2;
    while (true) {
        const int option = getopt_long(argc, argv, "i:uvh", NTA_INTERFACES_OPTIONS, NULL);
        if (option == -1) {
            break;
        }

        switch (option) {
        case 'i':
            opts.name = optarg;
            break;
        case 'u':
            opts.up_only = true;
            break;
        case 'v':
            opts.verbose = true;
            break;
        case 'j':
        case 'c':
        case 'x':
            nta_set_output(&opts.output, option);
            break;
        case 'h':
            nta_print_command_usage(stdout, argv[0], "interfaces");
            return NTA_PARSE_HELP;
        default:
            nta_print_command_usage(stderr, argv[0], "interfaces");
            return EXIT_FAILURE;
        }
    }

    if (nta_reject_extra_args(argc, argv, "interfaces") != EXIT_SUCCESS) {
        return EXIT_FAILURE;
    }

    return nta_print_interfaces(&opts);
}

static int nta_run_routes(int argc, char **argv)
{
    nta_routes_options opts = {
        .output = NTA_OUTPUT_TEXT,
        .table = NULL,
        .interface = NULL,
        .verbose = false,
    };

    optind = 2;
    while (true) {
        const int option = getopt_long(argc, argv, "T:i:vh", NTA_ROUTES_OPTIONS, NULL);
        if (option == -1) {
            break;
        }

        switch (option) {
        case 'T':
            opts.table = optarg;
            break;
        case 'i':
            opts.interface = optarg;
            break;
        case 'v':
            opts.verbose = true;
            break;
        case 'j':
        case 'c':
        case 'x':
            nta_set_output(&opts.output, option);
            break;
        case 'h':
            nta_print_command_usage(stdout, argv[0], "routes");
            return NTA_PARSE_HELP;
        default:
            nta_print_command_usage(stderr, argv[0], "routes");
            return EXIT_FAILURE;
        }
    }

    if (nta_reject_extra_args(argc, argv, "routes") != EXIT_SUCCESS) {
        return EXIT_FAILURE;
    }

    return nta_print_routes(&opts);
}

static int nta_run_dns(int argc, char **argv)
{
    nta_dns_options opts = {
        .output = NTA_OUTPUT_TEXT,
        .verbose = false,
    };

    optind = 2;
    while (true) {
        const int option = getopt_long(argc, argv, "vh", NTA_DNS_OPTIONS, NULL);
        if (option == -1) {
            break;
        }

        switch (option) {
        case 'v':
            opts.verbose = true;
            break;
        case 'j':
        case 'c':
        case 'x':
            nta_set_output(&opts.output, option);
            break;
        case 'h':
            nta_print_command_usage(stdout, argv[0], "dns");
            return NTA_PARSE_HELP;
        default:
            nta_print_command_usage(stderr, argv[0], "dns");
            return EXIT_FAILURE;
        }
    }

    if (nta_reject_extra_args(argc, argv, "dns") != EXIT_SUCCESS) {
        return EXIT_FAILURE;
    }

    return nta_print_dns(&opts);
}

static int nta_run_ping(int argc, char **argv)
{
    nta_ping_options opts = {
        .output = NTA_OUTPUT_TEXT,
        .target = NULL,
        .count = 4,
        .verbose = false,
    };

    optind = 2;
    while (true) {
        const int option = getopt_long(argc, argv, "t:n:vh", NTA_PING_OPTIONS, NULL);
        if (option == -1) {
            break;
        }

        switch (option) {
        case 't':
            opts.target = optarg;
            break;
        case 'n':
            if (nta_parse_count(optarg, &opts.count) != EXIT_SUCCESS) {
                return EXIT_FAILURE;
            }
            break;
        case 'v':
            opts.verbose = true;
            break;
        case 'j':
        case 'c':
        case 'x':
            nta_set_output(&opts.output, option);
            break;
        case 'h':
            nta_print_command_usage(stdout, argv[0], "ping");
            return NTA_PARSE_HELP;
        default:
            nta_print_command_usage(stderr, argv[0], "ping");
            return EXIT_FAILURE;
        }
    }

    if (nta_reject_extra_args(argc, argv, "ping") != EXIT_SUCCESS) {
        return EXIT_FAILURE;
    }

    if (opts.target == NULL) {
        fprintf(stderr, "netra: ping requires --target <host>\n\n");
        nta_print_command_usage(stderr, argv[0], "ping");
        return EXIT_FAILURE;
    }

    return nta_print_ping(&opts);
}

int nta_cli_run(int argc, char **argv)
{
    int result = EXIT_FAILURE;

    if (argc < 2 || strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0) {
        nta_print_usage(stdout, argv[0]);
        return EXIT_SUCCESS;
    }

    opterr = 0;

    if (!nta_is_command(argv[1])) {
        fprintf(stderr, "netra: unknown command '%s'\n\n", argv[1]);
        nta_print_usage(stderr, argv[0]);
        return EXIT_FAILURE;
    }

    if (strcmp(argv[1], "overview") == 0) {
        result = nta_run_overview(argc, argv);
    } else if (strcmp(argv[1], "connections") == 0) {
        result = nta_run_connections(argc, argv);
    } else if (strcmp(argv[1], "interfaces") == 0) {
        result = nta_run_interfaces(argc, argv);
    } else if (strcmp(argv[1], "routes") == 0) {
        result = nta_run_routes(argc, argv);
    } else if (strcmp(argv[1], "dns") == 0) {
        result = nta_run_dns(argc, argv);
    } else if (strcmp(argv[1], "ping") == 0) {
        result = nta_run_ping(argc, argv);
    }

    if (result == NTA_PARSE_HELP) {
        return EXIT_SUCCESS;
    }

    return result;
}
