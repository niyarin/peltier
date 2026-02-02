#include "nippy/nippy_parser.h"
#include "nippy/nippy_writer.h"
#include "edn/edn_parser.h"
#include "edn/edn_writer.h"
#include "arena.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>

#define VERSION "alpha"

// Subcommand type
typedef enum {
    CMD_NONE,
    CMD_THAW,
    CMD_FREEZE
} command_type_t;

// CLI options structure
typedef struct {
    command_type_t command;
    const char *input_filename;
    const char *output_filename;
    const char *selector;
    bool pretty_print;
    int indent_size;
    bool show_help;
    bool show_version;
} cli_options_t;

static void print_usage(const char *program_name) {
    printf("Usage: %s <command> [OPTIONS] [INPUT_FILE]\n", program_name);
    printf("\n");
    printf("Commands:\n");
    printf("  thaw        Convert Nippy binary format to EDN text format\n");
    printf("  freeze      Convert EDN text format to Nippy binary format\n");
    printf("\n");
    printf("Global Options:\n");
    printf("  -h, --help     Show this help message\n");
    printf("  -v, --version  Show version information\n");
    printf("\n");
    printf("Run '%s <command> --help' for command-specific options.\n", program_name);
    printf("\n");
    printf("Examples:\n");
    printf("  %s thaw input.nippy                    # Convert Nippy to EDN\n", program_name);
    printf("  %s thaw -p input.nippy                 # Pretty-print output\n", program_name);
    printf("  %s thaw -o output.edn input.nippy      # Write to file\n", program_name);
    printf("  %s freeze input.edn -o output.nippy    # Convert EDN to Nippy\n", program_name);
    printf("\n");
}

static void print_thaw_usage(const char *program_name) {
    printf("Usage: %s thaw [OPTIONS] [INPUT_FILE]\n", program_name);
    printf("\n");
    printf("Convert Nippy binary format to EDN text format.\n");
    printf("\n");
    printf("Options:\n");
    printf("  -o, --output FILE      Write output to FILE (default: stdout)\n");
    printf("  -p, --pretty           Pretty-print output with indentation\n");
    printf("  --indent N             Indentation size for pretty printing (default: 2)\n");
    printf("  -s, --select EXPR      Apply selector expression (not yet implemented)\n");
    printf("  -h, --help             Show this help message\n");
    printf("\n");
    printf("Examples:\n");
    printf("  %s thaw input.nippy\n", program_name);
    printf("  %s thaw -p input.nippy\n", program_name);
    printf("  %s thaw -o output.edn input.nippy\n", program_name);
    printf("  %s thaw < input.nippy > output.edn\n", program_name);
    printf("\n");
}

static void print_freeze_usage(const char *program_name) {
    printf("Usage: %s freeze [OPTIONS] [INPUT_FILE]\n", program_name);
    printf("\n");
    printf("Convert EDN text format to Nippy binary format.\n");
    printf("\n");
    printf("Options:\n");
    printf("  -o, --output FILE      Write output to FILE (default: stdout)\n");
    printf("  -h, --help             Show this help message\n");
    printf("\n");
}

static void print_version(void) {
    printf("peltier version %s\n", VERSION);
    printf("Nippy <-> EDN converter with streaming support\n");
}

// Parse command line arguments for thaw command
static int parse_thaw_options(int argc, char **argv, cli_options_t *options) {
    // Initialize defaults
    options->command = CMD_THAW;
    options->input_filename = NULL;
    options->output_filename = NULL;
    options->selector = NULL;
    options->pretty_print = false;
    options->indent_size = 2;
    options->show_help = false;
    options->show_version = false;

    static struct option long_options[] = {
        {"output",  required_argument, 0, 'o'},
        {"pretty",  no_argument,       0, 'p'},
        {"indent",  required_argument, 0, 'i'},
        {"select",  required_argument, 0, 's'},
        {"help",    no_argument,       0, 'h'},
        {0, 0, 0, 0}
    };

    int opt;
    int option_index = 0;

    // Reset getopt
    optind = 1;

    while ((opt = getopt_long(argc, argv, "o:ps:h", long_options, &option_index)) != -1) {
        switch (opt) {
            case 'o':
                options->output_filename = optarg;
                break;

            case 'p':
                options->pretty_print = true;
                break;

            case 'i':
                options->indent_size = atoi(optarg);
                if (options->indent_size < 0 || options->indent_size > 16) {
                    fprintf(stderr, "Error: Invalid indent size (must be 0-16)\n");
                    return -1;
                }
                break;

            case 's':
                options->selector = optarg;
                fprintf(stderr, "Warning: Selector support not yet implemented\n");
                break;

            case 'h':
                options->show_help = true;
                return 0;

            default:
                return -1;
        }
    }

    // Get input filename if provided
    if (optind < argc) {
        options->input_filename = argv[optind];
    }

    return 0;
}

// Parse command line arguments for freeze command
static int parse_freeze_options(int argc, char **argv, cli_options_t *options) {
    // Initialize defaults
    options->command = CMD_FREEZE;
    options->input_filename = NULL;
    options->output_filename = NULL;
    options->show_help = false;

    static struct option long_options[] = {
        {"output",  required_argument, 0, 'o'},
        {"help",    no_argument,       0, 'h'},
        {0, 0, 0, 0}
    };

    int opt;
    int option_index = 0;

    // Reset getopt
    optind = 1;

    while ((opt = getopt_long(argc, argv, "o:h", long_options, &option_index)) != -1) {
        switch (opt) {
            case 'o':
                options->output_filename = optarg;
                break;

            case 'h':
                options->show_help = true;
                return 0;

            default:
                return -1;
        }
    }

    // Get input filename if provided
    if (optind < argc) {
        options->input_filename = argv[optind];
    }

    return 0;
}

// Main conversion logic (thaw: Nippy -> EDN)
static int run_thaw(const cli_options_t *options) {
    int exit_code = 0;

    // Open input file or use stdin
    FILE *input = stdin;
    if (options->input_filename) {
        input = fopen(options->input_filename, "rb");
        if (!input) {
            fprintf(stderr, "Error: Cannot open input file '%s'\n", options->input_filename);
            return 1;
        }
    }

    // Open output file or use stdout
    FILE *output = stdout;
    if (options->output_filename) {
        output = fopen(options->output_filename, "w");
        if (!output) {
            fprintf(stderr, "Error: Cannot open output file '%s'\n", options->output_filename);
            if (input != stdin) fclose(input);
            return 1;
        }
    }

    // Create arena allocator
    arena_t *arena = arena_create(10 * 1024 * 1024);  // 10MB
    if (!arena) {
        fprintf(stderr, "Error: Failed to create memory arena\n");
        if (input != stdin) fclose(input);
        if (output != stdout) fclose(output);
        return 1;
    }

    // Create parser
    nippy_parser_t *parser = nippy_parser_create(input, arena);
    if (!parser) {
        fprintf(stderr, "Error: Failed to create parser\n");
        arena_destroy(arena);
        if (input != stdin) fclose(input);
        if (output != stdout) fclose(output);
        return 1;
    }

    // Create writer
    edn_writer_t *writer = edn_writer_create(output, options->pretty_print, options->indent_size);
    if (!writer) {
        fprintf(stderr, "Error: Failed to create writer\n");
        nippy_parser_destroy(parser);
        arena_destroy(arena);
        if (input != stdin) fclose(input);
        if (output != stdout) fclose(output);
        return 1;
    }

    // Main processing loop
    while (1) {
        const parse_event_t *event = nippy_parser_next_event(parser);

        if (event->type == EVENT_EOF) {
            break;
        }

        if (event->type == EVENT_ERROR) {
            fprintf(stderr, "Parse error at position %zu: %s\n",
                    nippy_parser_position(parser),
                    event->error_message ? event->error_message : "Unknown error");
            exit_code = 1;
            break;
        }

        // Write event to EDN output
        edn_writer_write_event(writer, event);
    }

    // Add newline at end if pretty printing
    if (options->pretty_print && exit_code == 0) {
        fprintf(output, "\n");
    }

    // Cleanup
    edn_writer_flush(writer);
    edn_writer_destroy(writer);
    nippy_parser_destroy(parser);
    arena_destroy(arena);

    if (input != stdin) fclose(input);
    if (output != stdout) fclose(output);

    return exit_code;
}

// Freeze logic (EDN -> Nippy)
static int run_freeze(const cli_options_t *options) {
    int exit_code = 0;

    // Open input file or use stdin
    FILE *input = stdin;
    if (options->input_filename) {
        input = fopen(options->input_filename, "r");
        if (!input) {
            fprintf(stderr, "Error: Cannot open input file '%s'\n", options->input_filename);
            return 1;
        }
    }

    // Open output file or use stdout
    FILE *output = stdout;
    if (options->output_filename) {
        output = fopen(options->output_filename, "wb");
        if (!output) {
            fprintf(stderr, "Error: Cannot open output file '%s'\n", options->output_filename);
            if (input != stdin) fclose(input);
            return 1;
        }
    }

    // Create arena allocator
    arena_t *arena = arena_create(10 * 1024 * 1024);  // 10MB
    if (!arena) {
        fprintf(stderr, "Error: Failed to create memory arena\n");
        if (input != stdin) fclose(input);
        if (output != stdout) fclose(output);
        return 1;
    }

    // Create EDN parser
    edn_parser_t *parser = edn_parser_create(input, arena);
    if (!parser) {
        fprintf(stderr, "Error: Failed to create EDN parser\n");
        arena_destroy(arena);
        if (input != stdin) fclose(input);
        if (output != stdout) fclose(output);
        return 1;
    }

    // Create Nippy writer
    nippy_writer_t *writer = nippy_writer_create(output);
    if (!writer) {
        fprintf(stderr, "Error: Failed to create Nippy writer\n");
        edn_parser_destroy(parser);
        arena_destroy(arena);
        if (input != stdin) fclose(input);
        if (output != stdout) fclose(output);
        return 1;
    }

    // Main processing loop
    while (1) {
        const parse_event_t *event = edn_parser_next_event(parser);

        if (event->type == EVENT_EOF) {
            break;
        }

        if (event->type == EVENT_ERROR) {
            fprintf(stderr, "Parse error: %s\n",
                    event->error_message ? event->error_message : "Unknown error");
            exit_code = 1;
            break;
        }

        // Write event to Nippy output
        if (!nippy_writer_write_event(writer, event)) {
            fprintf(stderr, "Write error: %s\n", nippy_writer_error(writer));
            exit_code = 1;
            break;
        }
    }

    // Cleanup
    nippy_writer_destroy(writer);
    edn_parser_destroy(parser);
    arena_destroy(arena);

    if (input != stdin) fclose(input);
    if (output != stdout) fclose(output);

    return exit_code;
}

int main(int argc, char **argv) {
    // Check for global options first
    if (argc > 1) {
        if (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0) {
            print_usage(argv[0]);
            return 0;
        }
        if (strcmp(argv[1], "--version") == 0 || strcmp(argv[1], "-v") == 0) {
            print_version();
            return 0;
        }
    }

    // Need at least one argument (the subcommand)
    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }

    // Parse subcommand
    const char *subcommand = argv[1];
    cli_options_t options = {0};

    if (strcmp(subcommand, "thaw") == 0) {
        // Parse thaw options
        int parse_result = parse_thaw_options(argc - 1, argv + 1, &options);
        if (parse_result < 0) {
            print_thaw_usage(argv[0]);
            return 1;
        }

        if (options.show_help) {
            print_thaw_usage(argv[0]);
            return 0;
        }

        return run_thaw(&options);

    } else if (strcmp(subcommand, "freeze") == 0) {
        // Parse freeze options
        int parse_result = parse_freeze_options(argc - 1, argv + 1, &options);
        if (parse_result < 0) {
            print_freeze_usage(argv[0]);
            return 1;
        }

        if (options.show_help) {
            print_freeze_usage(argv[0]);
            return 0;
        }

        return run_freeze(&options);

    } else {
        fprintf(stderr, "Error: Unknown command '%s'\n\n", subcommand);
        print_usage(argv[0]);
        return 1;
    }
}
