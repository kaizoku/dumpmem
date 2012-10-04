/* dumpmem.c
 * April 27th, 2008
 * kaizoku@phear.cc
 * Usage: dumpmem [-p pid] [-s executable]
 * Notes: Code inspired by, well..based on, er, actually stolen
 *        from from K-sPecial's little program to spawn a process
 *        and dump it's memory.
 */

#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <ctype.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/ptrace.h>

pid_t pid = 0;
bool Opt_ptrace = false;
bool Opt_spawn = false;
char *Opt_spawn_exec = NULL;
char **Opt_spawn_argv = NULL;
int Opt_spawn_argc = 0;

void usage(char *);
int start_tracing(void);
int stop_tracing(void);
int get_cmdline(pid_t, char *);
int parse_maps_line(char *);
bool getresp(char *);

int main(int argc, char **argv)
{
	if (argc < 3)
      {
		usage(argv[0]);
		exit(EXIT_FAILURE);
	  }

	// handle command line options
    extern int optind;
    extern char *optarg;

    int opt;
    while ((opt = getopt(argc, argv, "s:p:")) != -1)
      {
        switch (opt)
          {
            case 'p':
                Opt_ptrace = true;
                pid = (pid_t)atoi(optarg);
                break;
            case 's':
                Opt_spawn = true;
                Opt_spawn_exec = optarg;
                break;
            default:
                usage(argv[0]);
                exit(EXIT_FAILURE);
          }
      }
    Opt_spawn_argv = argv + optind;
    Opt_spawn_argc = argc - optind;

    // make sure both of these aren't set
    if (Opt_ptrace && Opt_spawn)
      {
        usage(argv[0]);
        exit(EXIT_FAILURE);
      }

    start_tracing();

    // open /proc/<pid>/maps
    char mapsfile[23];
	snprintf(mapsfile, 23, "/proc/%i/maps", (int)pid);
	printf("pid: %i\nmaps: %s\n", pid, mapsfile);
    FILE *map;
	map = fopen(mapsfile, "r");
	if (map == NULL)
        err(EXIT_FAILURE, "fopen");

    // main loop
    char *line;
	size_t length;
	while (getline(&line, &length, map) != -1)
      {
		printf("\n%s", line);
		if (getresp("parse this memory location? "))
            parse_maps_line(line);
	  }
    free(line);
	fclose(map);
    stop_tracing();
	return EXIT_SUCCESS;
}

int get_cmdline(pid_t pid, char *procname)
{
        char cmdline[23];
        snprintf(cmdline, 23, "/proc/%i/cmdline", pid);
        FILE *cmdln = fopen(cmdline, "r");
        if (cmdln == NULL)
            err(EXIT_FAILURE, "fopen");

        fgets(procname, sizeof(procname), cmdln);
        (procname[strlen(procname)-1] == '\n') ? (procname[strlen(procname)-1] = '\0') : 0;

        fclose(cmdln);
        return 0;
}

int parse_maps_line(char *line)
{
	unsigned int ini, fim;
    char file[128];
	FILE *outfile;
    long word;

    if (sscanf(line, "%x-%x", &ini, &fim) != 2)
      {
        fprintf(stderr, "FAILURE");
        return 1;
      }

    printf("Enter an output filename: ");
    fgets(file, 128, stdin);
    (file[strlen(file)-1] == '\n') ? (file[strlen(file)-1] = '\0') : 0;
    outfile = fopen(file, "w");
    if (outfile == NULL)
        err(EXIT_FAILURE, "fopen");

    while (ini <= fim)
      {
        word = ptrace(PT_READ_D, pid, ini, 0);
        if (word == -1)
          {
            warn("ptrace");
            break;
          }
        fwrite(&word, sizeof(word), 1, outfile);
        ini += sizeof(word);
      }
	fclose(outfile);
    return 0;
}

bool getresp(char *question)
{ 
    bool r;
    size_t len = 0;
    char *line = NULL;
    fwrite(question, strlen(question), 1, stdout);
    if (!getline(&line, &len, stdin))
        err(EXIT_FAILURE, "getline");
    *line = tolower(*line);

    if (*line == 'y')
        r = true;
    else
        r = false;

    free(line);
    return r;
}

int start_tracing(void)
{
    if (Opt_ptrace)
      {
        // attach to the pid
        char proc[128];
        get_cmdline(pid, proc);

        printf("Attaching to process: %s\n", proc);
        ptrace(PT_ATTACH, pid, 0, 0);
        ptrace(PT_CONTINUE, pid, 0, SIGCONT);
      }
    else if (Opt_spawn)
      {
        // spawn the new process
        pid = fork();
        if (pid == -1)
            err(EXIT_FAILURE, "fork");
        else if (pid == 0)
          {
            printf("Spawning process: %s\n", Opt_spawn_exec);
            int i;
            for (i = 0; i < Opt_spawn_argc; i++)
                printf("%s ", Opt_spawn_argv[i]);
            printf("\n");
            ptrace(PT_TRACE_ME, 0, NULL, NULL);
            execv(Opt_spawn_exec, Opt_spawn_argv); 
          }
      }
    return 0;
}

int stop_tracing(void)
{
    long retval = 0;
    if (Opt_ptrace)
        ptrace(PT_CONTINUE, pid, 0, SIGCONT);
    else if (Opt_spawn)
        retval = ptrace(PT_KILL, pid, 0, 0);

    if (retval == -1)
        err(EXIT_FAILURE, "ptrace");
    else
        return 0;
}

void usage(char *progname)
{
	fprintf(stderr, "Usage: %s [-p PID] [-s executable]\n", progname);
}
