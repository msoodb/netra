#include "nta_cli.h"

#include <getopt.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define NTA_PARSE_HELP 2
#define NTA_MAX_OPTIONS 16

typedef enum {
    NTA_OUTPUT_TEXT = 0,
    NTA_OUTPUT_JSON,
    NTA_OUTPUT_CSV,
} nta_output_format;

typedef struct {
    const char *name;
    int short_name;
    bool has_value;
    const char *value_name;
    const char *value_kind;
    const char *summary;
} nta_option_spec;

typedef struct {
    const char *name;
    const char *summary;
    const char *usage;
    const nta_option_spec *options;
    size_t option_count;
} nta_command_spec;

typedef struct {
    nta_output_format output;
} nta_overview_options;

typedef struct {
    nta_output_format output;
    const char *pid;
    const char *interface;
    bool listening;
} nta_connections_options;

typedef struct {
    nta_output_format output;
    const char *name;
    bool up_only;
} nta_interfaces_options;

typedef struct {
    nta_output_format output;
    const char *table;
    const char *interface;
} nta_routes_options;

typedef struct {
    nta_output_format output;
} nta_dns_options;

typedef struct {
    nta_output_format output;
    const char *target;
    int count;
} nta_ping_options;

static const nta_option_spec NTA_OUTPUT_OPTIONS[] = {
    {"json", 'j', false, NULL, NULL, "Print JSON output"},
    {"csv", 'c', false, NULL, NULL, "Print CSV output"},
    {"text", 'x', false, NULL, NULL, "Print human-readable output"},
};

static const nta_option_spec NTA_CONNECTIONS_OPTIONS[] = {
    {"pid", 'p', true, "pid", "pid", "Filter by process ID"},
    {"interface", 'i', true, "name", "interface", "Filter by interface name"},
    {"listening", 'l', false, NULL, NULL, "Show listening sockets only"},
};

static const nta_option_spec NTA_INTERFACES_OPTIONS[] = {
    {"name", 'i', true, "name", "interface", "Show one interface"},
    {"up", 'u', false, NULL, NULL, "Show interfaces that are up only"},
};

static const nta_option_spec NTA_ROUTES_OPTIONS[] = {
    {"table", 'T', true, "table", "table", "Route table name or ID"},
    {"interface", 'i', true, "name", "interface", "Filter by interface name"},
};

static const nta_option_spec NTA_PING_OPTIONS[] = {
    {"target", 't', true, "host", "host", "Target host or IP address"},
    {"count", 'n', true, "count", "count", "Number of probes"},
};

static const nta_command_spec NTA_COMMANDS[] = {
    {"overview", "Show a high-level network summary", "overview [--json|--csv|--text]", NULL, 0},
    {"connections", "List active network connections", "connections [options] [--json|--csv|--text]",
     NTA_CONNECTIONS_OPTIONS, sizeof(NTA_CONNECTIONS_OPTIONS) / sizeof(NTA_CONNECTIONS_OPTIONS[0])},
    {"interfaces", "List network interfaces", "interfaces [options] [--json|--csv|--text]",
     NTA_INTERFACES_OPTIONS, sizeof(NTA_INTERFACES_OPTIONS) / sizeof(NTA_INTERFACES_OPTIONS[0])},
    {"routes", "Show routing information", "routes [options] [--json|--csv|--text]",
     NTA_ROUTES_OPTIONS, sizeof(NTA_ROUTES_OPTIONS) / sizeof(NTA_ROUTES_OPTIONS[0])},
    {"dns", "Show DNS resolver information", "dns [--json|--csv|--text]", NULL, 0},
    {"ping", "Run a basic reachability diagnostic", "ping --target <host> [options] [--json|--csv|--text]",
     NTA_PING_OPTIONS, sizeof(NTA_PING_OPTIONS) / sizeof(NTA_PING_OPTIONS[0])},
};

static const size_t NTA_COMMAND_COUNT = sizeof(NTA_COMMANDS) / sizeof(NTA_COMMANDS[0]);
static const size_t NTA_OUTPUT_OPTION_COUNT = sizeof(NTA_OUTPUT_OPTIONS) / sizeof(NTA_OUTPUT_OPTIONS[0]);

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

static const nta_command_spec *nta_find_command(const char *name)
{
    for (size_t i = 0; i < NTA_COMMAND_COUNT; ++i) {
        if (strcmp(name, NTA_COMMANDS[i].name) == 0) {
            return &NTA_COMMANDS[i];
        }
    }

    return NULL;
}

static const nta_option_spec *nta_find_option(const nta_command_spec *command, const char *option)
{
    const char *name = option;

    if (strncmp(name, "--", 2) == 0) {
        name += 2;
        for (size_t i = 0; i < command->option_count; ++i) {
            if (strcmp(name, command->options[i].name) == 0) {
                return &command->options[i];
            }
        }
        for (size_t i = 0; i < NTA_OUTPUT_OPTION_COUNT; ++i) {
            if (strcmp(name, NTA_OUTPUT_OPTIONS[i].name) == 0) {
                return &NTA_OUTPUT_OPTIONS[i];
            }
        }
        return NULL;
    }

    if (name[0] == '-' && name[1] != '\0' && name[2] == '\0') {
        const int short_name = name[1];
        for (size_t i = 0; i < command->option_count; ++i) {
            if (command->options[i].short_name == short_name) {
                return &command->options[i];
            }
        }
        for (size_t i = 0; i < NTA_OUTPUT_OPTION_COUNT; ++i) {
            if (NTA_OUTPUT_OPTIONS[i].short_name == short_name) {
                return &NTA_OUTPUT_OPTIONS[i];
            }
        }
    }

    return NULL;
}

static void nta_print_usage(FILE *stream, const char *program)
{
    fprintf(stream, "Usage: %s <command> [options]\n\n", program);
    fprintf(stream, "Commands:\n");
    for (size_t i = 0; i < NTA_COMMAND_COUNT; ++i) {
        fprintf(stream, "  %-12s %s\n", NTA_COMMANDS[i].name, NTA_COMMANDS[i].summary);
    }
    fprintf(stream, "\nRun '%s <command> --help' for command-specific options.\n", program);
}

static void nta_print_option_usage(FILE *stream, const nta_option_spec *option)
{
    if (option->has_value) {
        fprintf(stream, "  -%c, --%s <%s>   %s\n",
                option->short_name,
                option->name,
                option->value_name,
                option->summary);
    } else {
        fprintf(stream, "  -%c, --%s       %s\n",
                option->short_name,
                option->name,
                option->summary);
    }
}

static void nta_print_command_usage(FILE *stream, const char *program, const nta_command_spec *command)
{
    fprintf(stream, "Usage: %s %s\n", program, command->usage);

    if (command->option_count > 0) {
        fprintf(stream, "\nCommand options:\n");
        for (size_t i = 0; i < command->option_count; ++i) {
            nta_print_option_usage(stream, &command->options[i]);
        }
    }

    fprintf(stream, "\nOutput options:\n");
    for (size_t i = 0; i < NTA_OUTPUT_OPTION_COUNT; ++i) {
        nta_print_option_usage(stream, &NTA_OUTPUT_OPTIONS[i]);
    }
}

static void nta_add_getopt_option(const nta_option_spec *option,
                                  struct option *long_options,
                                  size_t *long_count,
                                  char *short_options,
                                  size_t short_capacity)
{
    const size_t short_len = strlen(short_options);

    long_options[*long_count] = (struct option){
        .name = option->name,
        .has_arg = option->has_value ? required_argument : no_argument,
        .flag = NULL,
        .val = option->short_name,
    };
    ++(*long_count);

    if (short_len + (option->has_value ? 2U : 1U) + 1U >= short_capacity) {
        return;
    }

    short_options[short_len] = (char)option->short_name;
    if (option->has_value) {
        short_options[short_len + 1U] = ':';
        short_options[short_len + 2U] = '\0';
    } else {
        short_options[short_len + 1U] = '\0';
    }
}

static void nta_prepare_getopt_options(const nta_command_spec *command,
                                       struct option *long_options,
                                       char *short_options,
                                       size_t short_capacity)
{
    size_t long_count = 0;

    short_options[0] = '\0';

    for (size_t i = 0; i < command->option_count; ++i) {
        nta_add_getopt_option(&command->options[i], long_options, &long_count, short_options, short_capacity);
    }
    for (size_t i = 0; i < NTA_OUTPUT_OPTION_COUNT; ++i) {
        nta_add_getopt_option(&NTA_OUTPUT_OPTIONS[i], long_options, &long_count, short_options, short_capacity);
    }

    long_options[long_count] = (struct option){
        .name = "help",
        .has_arg = no_argument,
        .flag = NULL,
        .val = 'h',
    };
    ++long_count;
    long_options[long_count] = (struct option){0};

    strncat(short_options, "h", short_capacity - strlen(short_options) - 1U);
}

static int nta_reject_extra_args(int argc, char **argv, const nta_command_spec *command)
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
        printf(",\n  \"listening\": %s,\n  \"status\": \"stub\"\n}\n", opts->listening ? "true" : "false");
    } else if (opts->output == NTA_OUTPUT_CSV) {
        printf("command,format,pid,interface,listening,status\n");
        printf("connections,csv,%s,%s,%s,stub\n",
               opts->pid != NULL ? opts->pid : "",
               opts->interface != NULL ? opts->interface : "",
               opts->listening ? "true" : "false");
    } else {
        nta_print_stub_text("connections", opts->output);
        printf("  pid:       %s\n", opts->pid != NULL ? opts->pid : "-");
        printf("  interface: %s\n", opts->interface != NULL ? opts->interface : "-");
        printf("  listening: %s\n", opts->listening ? "yes" : "no");
    }

    return EXIT_SUCCESS;
}

static int nta_print_interfaces(const nta_interfaces_options *opts)
{
    if (opts->output == NTA_OUTPUT_JSON) {
        printf("{\n  \"command\": \"interfaces\",\n  \"format\": \"json\",\n");
        printf("  \"name\": ");
        nta_print_json_string(opts->name);
        printf(",\n  \"up_only\": %s,\n  \"status\": \"stub\"\n}\n", opts->up_only ? "true" : "false");
    } else if (opts->output == NTA_OUTPUT_CSV) {
        printf("command,format,name,up_only,status\n");
        printf("interfaces,csv,%s,%s,stub\n",
               opts->name != NULL ? opts->name : "",
               opts->up_only ? "true" : "false");
    } else {
        nta_print_stub_text("interfaces", opts->output);
        printf("  name:    %s\n", opts->name != NULL ? opts->name : "-");
        printf("  up only: %s\n", opts->up_only ? "yes" : "no");
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
        printf(",\n  \"status\": \"stub\"\n}\n");
    } else if (opts->output == NTA_OUTPUT_CSV) {
        printf("command,format,table,interface,status\n");
        printf("routes,csv,%s,%s,stub\n",
               opts->table != NULL ? opts->table : "",
               opts->interface != NULL ? opts->interface : "");
    } else {
        nta_print_stub_text("routes", opts->output);
        printf("  table:     %s\n", opts->table != NULL ? opts->table : "-");
        printf("  interface: %s\n", opts->interface != NULL ? opts->interface : "-");
    }

    return EXIT_SUCCESS;
}

static int nta_print_dns(const nta_dns_options *opts)
{
    if (opts->output == NTA_OUTPUT_JSON) {
        printf("{\n  \"command\": \"dns\",\n  \"format\": \"json\",\n  \"status\": \"stub\"\n}\n");
    } else if (opts->output == NTA_OUTPUT_CSV) {
        printf("command,format,status\n");
        printf("dns,csv,stub\n");
    } else {
        nta_print_stub_text("dns", opts->output);
    }

    return EXIT_SUCCESS;
}

static int nta_print_ping(const nta_ping_options *opts)
{
    if (opts->output == NTA_OUTPUT_JSON) {
        printf("{\n  \"command\": \"ping\",\n  \"format\": \"json\",\n");
        printf("  \"target\": ");
        nta_print_json_string(opts->target);
        printf(",\n  \"count\": %d,\n  \"status\": \"stub\"\n}\n", opts->count);
    } else if (opts->output == NTA_OUTPUT_CSV) {
        printf("command,format,target,count,status\n");
        printf("ping,csv,%s,%d,stub\n",
               opts->target != NULL ? opts->target : "",
               opts->count);
    } else {
        nta_print_stub_text("ping", opts->output);
        printf("  target: %s\n", opts->target != NULL ? opts->target : "-");
        printf("  count:  %d\n", opts->count);
    }

    return EXIT_SUCCESS;
}

static int nta_run_overview(int argc, char **argv, const nta_command_spec *command)
{
    nta_overview_options opts = {.output = NTA_OUTPUT_TEXT};
    struct option long_options[NTA_MAX_OPTIONS] = {0};
    char short_options[NTA_MAX_OPTIONS * 2] = {0};

    nta_prepare_getopt_options(command, long_options, short_options, sizeof(short_options));
    optind = 2;
    while (true) {
        const int option = getopt_long(argc, argv, short_options, long_options, NULL);
        if (option == -1) {
            break;
        }

        if (option == 'j' || option == 'c' || option == 'x') {
            nta_set_output(&opts.output, option);
        } else if (option == 'h') {
            nta_print_command_usage(stdout, argv[0], command);
            return NTA_PARSE_HELP;
        } else {
            nta_print_command_usage(stderr, argv[0], command);
            return EXIT_FAILURE;
        }
    }

    if (nta_reject_extra_args(argc, argv, command) != EXIT_SUCCESS) {
        return EXIT_FAILURE;
    }

    return nta_print_overview(&opts);
}

static int nta_run_connections(int argc, char **argv, const nta_command_spec *command)
{
    nta_connections_options opts = {
        .output = NTA_OUTPUT_TEXT,
        .pid = NULL,
        .interface = NULL,
        .listening = false,
    };
    struct option long_options[NTA_MAX_OPTIONS] = {0};
    char short_options[NTA_MAX_OPTIONS * 2] = {0};

    nta_prepare_getopt_options(command, long_options, short_options, sizeof(short_options));
    optind = 2;
    while (true) {
        const int option = getopt_long(argc, argv, short_options, long_options, NULL);
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
        case 'j':
        case 'c':
        case 'x':
            nta_set_output(&opts.output, option);
            break;
        case 'h':
            nta_print_command_usage(stdout, argv[0], command);
            return NTA_PARSE_HELP;
        default:
            nta_print_command_usage(stderr, argv[0], command);
            return EXIT_FAILURE;
        }
    }

    if (nta_reject_extra_args(argc, argv, command) != EXIT_SUCCESS) {
        return EXIT_FAILURE;
    }

    return nta_print_connections(&opts);
}

static int nta_run_interfaces(int argc, char **argv, const nta_command_spec *command)
{
    nta_interfaces_options opts = {
        .output = NTA_OUTPUT_TEXT,
        .name = NULL,
        .up_only = false,
    };
    struct option long_options[NTA_MAX_OPTIONS] = {0};
    char short_options[NTA_MAX_OPTIONS * 2] = {0};

    nta_prepare_getopt_options(command, long_options, short_options, sizeof(short_options));
    optind = 2;
    while (true) {
        const int option = getopt_long(argc, argv, short_options, long_options, NULL);
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
        case 'j':
        case 'c':
        case 'x':
            nta_set_output(&opts.output, option);
            break;
        case 'h':
            nta_print_command_usage(stdout, argv[0], command);
            return NTA_PARSE_HELP;
        default:
            nta_print_command_usage(stderr, argv[0], command);
            return EXIT_FAILURE;
        }
    }

    if (nta_reject_extra_args(argc, argv, command) != EXIT_SUCCESS) {
        return EXIT_FAILURE;
    }

    return nta_print_interfaces(&opts);
}

static int nta_run_routes(int argc, char **argv, const nta_command_spec *command)
{
    nta_routes_options opts = {
        .output = NTA_OUTPUT_TEXT,
        .table = NULL,
        .interface = NULL,
    };
    struct option long_options[NTA_MAX_OPTIONS] = {0};
    char short_options[NTA_MAX_OPTIONS * 2] = {0};

    nta_prepare_getopt_options(command, long_options, short_options, sizeof(short_options));
    optind = 2;
    while (true) {
        const int option = getopt_long(argc, argv, short_options, long_options, NULL);
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
        case 'j':
        case 'c':
        case 'x':
            nta_set_output(&opts.output, option);
            break;
        case 'h':
            nta_print_command_usage(stdout, argv[0], command);
            return NTA_PARSE_HELP;
        default:
            nta_print_command_usage(stderr, argv[0], command);
            return EXIT_FAILURE;
        }
    }

    if (nta_reject_extra_args(argc, argv, command) != EXIT_SUCCESS) {
        return EXIT_FAILURE;
    }

    return nta_print_routes(&opts);
}

static int nta_run_dns(int argc, char **argv, const nta_command_spec *command)
{
    nta_dns_options opts = {.output = NTA_OUTPUT_TEXT};
    struct option long_options[NTA_MAX_OPTIONS] = {0};
    char short_options[NTA_MAX_OPTIONS * 2] = {0};

    nta_prepare_getopt_options(command, long_options, short_options, sizeof(short_options));
    optind = 2;
    while (true) {
        const int option = getopt_long(argc, argv, short_options, long_options, NULL);
        if (option == -1) {
            break;
        }

        if (option == 'j' || option == 'c' || option == 'x') {
            nta_set_output(&opts.output, option);
        } else if (option == 'h') {
            nta_print_command_usage(stdout, argv[0], command);
            return NTA_PARSE_HELP;
        } else {
            nta_print_command_usage(stderr, argv[0], command);
            return EXIT_FAILURE;
        }
    }

    if (nta_reject_extra_args(argc, argv, command) != EXIT_SUCCESS) {
        return EXIT_FAILURE;
    }

    return nta_print_dns(&opts);
}

static int nta_run_ping(int argc, char **argv, const nta_command_spec *command)
{
    nta_ping_options opts = {
        .output = NTA_OUTPUT_TEXT,
        .target = NULL,
        .count = 4,
    };
    struct option long_options[NTA_MAX_OPTIONS] = {0};
    char short_options[NTA_MAX_OPTIONS * 2] = {0};

    nta_prepare_getopt_options(command, long_options, short_options, sizeof(short_options));
    optind = 2;
    while (true) {
        const int option = getopt_long(argc, argv, short_options, long_options, NULL);
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
        case 'j':
        case 'c':
        case 'x':
            nta_set_output(&opts.output, option);
            break;
        case 'h':
            nta_print_command_usage(stdout, argv[0], command);
            return NTA_PARSE_HELP;
        default:
            nta_print_command_usage(stderr, argv[0], command);
            return EXIT_FAILURE;
        }
    }

    if (nta_reject_extra_args(argc, argv, command) != EXIT_SUCCESS) {
        return EXIT_FAILURE;
    }

    if (opts.target == NULL) {
        fprintf(stderr, "netra: ping requires --target <host>\n\n");
        nta_print_command_usage(stderr, argv[0], command);
        return EXIT_FAILURE;
    }

    return nta_print_ping(&opts);
}

static int nta_print_completion_commands(void)
{
    for (size_t i = 0; i < NTA_COMMAND_COUNT; ++i) {
        printf("%s\n", NTA_COMMANDS[i].name);
    }
    printf("-h\n--help\n");
    return EXIT_SUCCESS;
}

static int nta_print_completion_options(const nta_command_spec *command)
{
    for (size_t i = 0; i < command->option_count; ++i) {
        printf("-%c\n--%s\n", command->options[i].short_name, command->options[i].name);
    }
    for (size_t i = 0; i < NTA_OUTPUT_OPTION_COUNT; ++i) {
        printf("-%c\n--%s\n", NTA_OUTPUT_OPTIONS[i].short_name, NTA_OUTPUT_OPTIONS[i].name);
    }
    printf("-h\n--help\n");
    return EXIT_SUCCESS;
}

static int nta_print_completion_value_kind(const nta_command_spec *command, const char *option)
{
    const nta_option_spec *spec = nta_find_option(command, option);

    if (spec == NULL || !spec->has_value || spec->value_kind == NULL) {
        return EXIT_SUCCESS;
    }

    printf("%s\n", spec->value_kind);
    return EXIT_SUCCESS;
}

static int nta_run_completion(int argc, char **argv)
{
    const nta_command_spec *command = NULL;

    if (argc < 3) {
        return EXIT_FAILURE;
    }

    if (strcmp(argv[2], "commands") == 0) {
        return nta_print_completion_commands();
    }

    if (argc < 4) {
        return EXIT_FAILURE;
    }

    command = nta_find_command(argv[3]);
    if (command == NULL) {
        return EXIT_SUCCESS;
    }

    if (strcmp(argv[2], "options") == 0) {
        return nta_print_completion_options(command);
    }

    if (strcmp(argv[2], "value-kind") == 0) {
        if (argc < 5) {
            return EXIT_FAILURE;
        }
        return nta_print_completion_value_kind(command, argv[4]);
    }

    return EXIT_FAILURE;
}

int nta_cli_run(int argc, char **argv)
{
    const nta_command_spec *command = NULL;
    int result = EXIT_FAILURE;

    if (argc >= 2 && strcmp(argv[1], "__complete") == 0) {
        return nta_run_completion(argc, argv);
    }

    if (argc < 2 || strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0) {
        nta_print_usage(stdout, argv[0]);
        return EXIT_SUCCESS;
    }

    opterr = 0;

    command = nta_find_command(argv[1]);
    if (command == NULL) {
        fprintf(stderr, "netra: unknown command '%s'\n\n", argv[1]);
        nta_print_usage(stderr, argv[0]);
        return EXIT_FAILURE;
    }

    if (strcmp(command->name, "overview") == 0) {
        result = nta_run_overview(argc, argv, command);
    } else if (strcmp(command->name, "connections") == 0) {
        result = nta_run_connections(argc, argv, command);
    } else if (strcmp(command->name, "interfaces") == 0) {
        result = nta_run_interfaces(argc, argv, command);
    } else if (strcmp(command->name, "routes") == 0) {
        result = nta_run_routes(argc, argv, command);
    } else if (strcmp(command->name, "dns") == 0) {
        result = nta_run_dns(argc, argv, command);
    } else if (strcmp(command->name, "ping") == 0) {
        result = nta_run_ping(argc, argv, command);
    }

    if (result == NTA_PARSE_HELP) {
        return EXIT_SUCCESS;
    }

    return result;
}
