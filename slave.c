/**
 * Slave - replaces the current process with a new process (argument)
 */
#define _POSIX_C_SOURCE 2
#include <stdio.h>
#include <unistd.h>

int exec(int argc, char *argv[])
{
	if (argc > 0) {
		execvp(argv[0], argv);

		perror("Cannot run the requested command");

		return -1;
	}

	fprintf(stderr, "Nothing to do\n");

	return -1;
}

int main(int argc, char *argv[])
{
	parse: switch (getopt(argc, argv, "o:e:d:")) {
		case 'o':
			if (freopen(optarg, "w", stdout) == NULL) {
				perror("Cannot open output file");

				return -1;
			}
			goto parse;
		case 'e':
			if (freopen(optarg, "w", stderr) == NULL) {
				perror("Cannot open error file");

				return -1;
			}
			goto parse;
		case 'd':
			if (chdir(optarg) < 0) {
				perror("Cannot change working directory");

				return -1;
			}
			goto parse;
		default:
			return -1;
		case -1:
			;
	}

	return exec(argc - optind, argv + optind);
}
