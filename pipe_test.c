/*
 * pipe_test ... a program for driving test input through a pipe pair
 *
 *	1. create a pipe pair
 *	2. start a designated program on the slave side
 *	3. process commands to send and validate input
 *
 * Usage:
 *	pipe_test [--verbose] [--script=file] program args ...
 *
 * Command language:
 *	SEND "..."
 *	EXPECT "..."
 *	WAIT [maxwait] (for expectation)
 *	PAUSE waittime
 *	CLOSE	(shut down and collect child status)
 *
 *   if no --script is provided, commands read from stdin
 *
 * Escape conventions in send/expect
 *	graphics	represent themselves
 *	^x		control what ever
 *	\n \r \t	the obvious
 *	^^, \\, \"	literal escapes
 *
 * Output:
 *	everything received goes to stdout
 *	diagnostic output goes to stderr
 *	it exits with the status of the sub-process
 *		... or -1 for bad args or wait timeout
 * TODO
 *	who is leaving orphans, and why
 *	preempt WAIT when EXPECT is satisfied
 *	enable SIGCHLD to gather status
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/fcntl.h>
#include <errno.h>
#include <string.h>
#include <pthread.h>
#include <signal.h>
#include <stdint.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

#define MAXARGS	10
#define	BUFSIZE 512

#define	EXIT_BADARG	-1
#define	EXIT_NOCHILD	-1
#define	EXIT_EXPECT	-2

const char *usage = "[-v] [--script=file] [--timeout=seconds] [--port=# [--host=name]] [program [args ...]]";

int timeout = 0;	/* timeout alarm interval	*/
int verbose = 0;	/* enable debug output		*/
int inPipe[2];		/* pipe in from sub-process	*/
int outPipe[2];		/* pipe out to sub-process	*/

/* communication between command and output threads	*/
char *expecting = 0;	/* expecting input		*/
int writer_stop = 0;	/* shut down 			*/
pid_t child_pid = 0;	/* child process 		*/
int child_status = -1;	/* child exit status		*/

/* 
 * signal handler ... just for wakeups
 * 	just return
 */
void handler( int sig ) {
}

/*
 * alarm handler ... this is taking too long
 */
void time_handler( int sig ) {
	writer_stop = 1;
	fprintf(stderr, "!!! ALARM !!! Killing Child process %d\n", child_pid);
	kill(child_pid, SIGTERM);
}

/*
 * sigchild handler ... collect status
 */
void child_handler( int sig ) {
	if (verbose)
		fprintf(stderr, "\n... SIGCHLD: %d has exited\n", child_pid);
	if (child_status == -1) {
		waitpid(child_pid, &child_status, 0);
		if (verbose)
			fprintf(stderr, "... exit status: %d\n", child_status);
	} else if (verbose) 
		fprintf(stderr, "... status already collected: %d\n", child_status);
}

/*
 * process_output ... copy data coming back from pty to stdout
 *
 * @return	0: normal shutdown
 *		-1: unexpected read error from pty
 */
void *process_output( void *unused ) {
	char inbuf[BUFSIZE];
	int infd = inPipe[0];

	uintptr_t ret = 0;
	signal(SIGTERM, handler);

	/* read until their ain't no more	*/
	for(;;) {
		if (verbose)
			fprintf(stderr, "... writer TOP OF LOOP stop=%d, sts=%d\n", 
				writer_stop, child_status);
		int cnt = read(infd, inbuf, sizeof inbuf);
		if (verbose)
			fprintf(stderr, "... writer: read(%d) -> %d\n", infd, cnt);
		if (cnt <= 0) {
			if (verbose)
				fprintf(stderr, "... output thread got EOF\n");
			if (!writer_stop)
				ret = -1;
			break;
		}

		/* tick off expected characters		*/
		if (expecting) {
			char *s = inbuf;
			while( *expecting && s < &inbuf[cnt] ) {
				if (*s++ == *expecting)
					expecting++;
			}

			/* see if we are done	*/
			if (*expecting == 0) {
				expecting = 0;
				if (verbose)
					fprintf(stderr, "... expectation satisfied\n");
			}
		}

		/* and forward all output to stdout	*/
		int rc = write(1, inbuf, cnt);
	}
	
	if (verbose)
		fprintf(stderr, "... writer thread exiting (ret=%lu)\n", ret);
	pthread_exit((void *) ret);
}

/*
 * skip leading whitespace in a null terminated string
 */
const char *skipWhite( const char *s ) {
	while( *s == ' ' || *s == '\t' || *s == '\n')
		s++;
	return(s);
}

/*
 * canonize ... interpret escape sequences in a string
 *
 *	string is either quote or space delimited
 *	special characters are \r \n \t or ^letter
 #	a backslash escapes anything else
 *
 *	since we don't expect to send nulls, both
 *	input and output strings are null terminated.
 *
 * @parmam	source string
 * @param	destination buffer
 * 
 * @return	character after source string
 */
const char *canonize( const char *src, char *dest ) {
	char quote;
	if (*src == '\'' || *src == '"')
		quote = *src++;
	else
		quote = ' ';

	while(*src && *src != '\n' && *src != quote) {
		if (*src == '^') {
			*dest++ = src[1] - '@';
			src += 2;
		} else if (*src == '\\') {
			if (src[1] == 'r')
				*dest++ = '\r';
			else if (src[1] == 'n')
				*dest++ = '\n';
			else if (src[1] == 't')
				*dest++ = '\t';
			else
				*dest++ = src[1];
			src += 2;
		} else
			*dest++ = *src++;
	}
	*dest++ = 0;

	return( (*src == quote) ? &src[1] : src );
}

/*
 * escape ... oppositie of canonize
 *		interpret non-graphics
 */
const char *escape( const char *input ) {
	static char outbuf[BUFSIZE];

	char *p = outbuf;
	while( *input ) {
		if (*input < ' ') {
			*p++ = '^';
			*p++ = *input++ + '@';
		} else if (*input == 0x7f) {
			*p++ = '\\';
			*p++ = '1';
			*p++ = '7';
			*p++ = '7';
			input++;
		} else
			*p++ = *input++;
	}
	*p++ = 0;

	return outbuf;
}


/*
 * process_command ... drive pty input
 *
 * @param command to be executed
 *
 * @return (boolean) continue processing
 */
int process_command(const char *cmd) {
	static char sendbuf[BUFSIZE];
	static char expbuf[BUFSIZE];
	int ret;

 	if (strncmp("SEND", cmd, 4) == 0) {
		const char *s = skipWhite(&cmd[4]);
		s = canonize(s, sendbuf);
		if (verbose)
			fprintf(stderr, "... SEND \"%s\"\n", escape(sendbuf));
		ret = write(outPipe[1], sendbuf, strlen(sendbuf));
		if (ret != strlen(sendbuf))
			fprintf(stderr, "... write(%d,%d) -> %d\n", outPipe[1], (int) strlen(sendbuf), ret);
		return(1);
	} else if (strncmp("EXPECT", cmd, 6) == 0) {
		const char *s = skipWhite(&cmd[6]);
		s = canonize(s, expbuf);
		expecting = expbuf;
		if (verbose)
			fprintf(stderr, "... EXPECT \"%s\"\n", escape(expbuf));
		return(1);
	} else if (strncmp("WAIT", cmd, 4) == 0) {
		const char *s = skipWhite(&cmd[4]);
		int delay = atoi(s);
		if (delay > 0) {
			if (verbose)
				fprintf(stderr, "... WAIT %ds\n", delay);
			if (expecting) {
				sleep(delay);
				if (expecting) {
					fprintf(stderr, "EXPECTATION NOT FULFILLED\n");
					exit(EXIT_EXPECT);
				}
			} else if (verbose)
				fprintf(stderr, "... expectation already fulfilled\n");
		}
		return(1);
	} else if (strncmp("PAUSE", cmd, 5) == 0) {
		const char *s = skipWhite(&cmd[5]);
		int delay = atoi(s);
		if (delay > 0) {
			if (verbose)
				fprintf(stderr, "... PAUSE %ds\n", delay);
			sleep(delay);
		}
		return(1);
	} else if (strncmp("CLOSE", cmd, 5) == 0) {
		if (verbose)
			fprintf(stderr, "... CLOSE, waiting for %d to exit\n", child_pid);
		// make it clear nothing else is coming
		close(outPipe[1]);

		int rc = waitpid(child_pid, &child_status, 0);
		fprintf(stderr, "... waitpid returns %d, status=%d\n", rc, child_status);
		return( 0 );
	}

	fprintf(stderr, "Unrecognized command: %s\n", cmd);
		return( 0 );
}

/*
 * process arguments, open the pipes, spawn process at 
 * the other end, and run the main data-passing loop
 */
int main( int argc, char **argv ) {
	/* process initial arguments for me	*/
	const char *script = 0;
	const char *host = 0;
	const char *program = 0;
	int port = 0;
	int argx;
	for( argx = 1; argx < argc; argx++ ) {
		char *s = argv[argx];
		if (s[0] == '-') {
			if (s[1] == 'v' || strcmp(s, "--verbose") == 0) {
				verbose = 1;
				continue;
			}
			if (s[1] == 's') {
				script = s[2] ? &s[2] : argv[++argx];
				continue;
			}
			if (strncmp(s, "--script", 8) == 0) {
				script = (s[8] == '=') ? &s[9] : argv[++argx];
				continue;
			}
			if (s[1] == 't') {
				timeout = atoi( s[2] ? &s[2] : argv[++argx]);
				continue;
			}
			if (strncmp(s, "--timeout", 9) == 0) {
				timeout = atoi(( s[9] == '=') ? &s[10] : argv[++argx]);
				continue;
			}
			if (strncmp(s, "--port", 6) == 0) {
				port = atoi(( s[6] == '=') ? &s[7] : argv[++argx]);
				continue;
			}
			if (strncmp(s, "--host", 6) == 0) {
				host = (s[6] == '=') ? &s[7] : argv[++argx];
				continue;
			}
			else {
				fprintf(stderr, "Unrecognized argument: %s\n", s);
				fprintf(stderr, "Usage: %s %s\n", argv[0], usage);
				exit(EXIT_BADARG);
			}
		} else 
			break;
	}
	
	if (verbose) {
		fprintf(stderr, "%s", argv[0]);
		if (verbose)
			fprintf(stderr, " --verbose");
		if (timeout)
			fprintf(stderr, " --timeout=%d", timeout);
		if (script)
			fprintf(stderr, " --script=%s", script);
		if (port)
			fprintf(stderr, " --port=%d", port);
		if (host)
			fprintf(stderr, " --host=%s", host);
		fprintf(stderr, "\n");
	}

	/* make sure we have a program to run */
	if (argx >= argc && port == 0) {
		fprintf(stderr, "No program or port specified\n");
				fprintf(stderr, "Usage: %s %s\n", argv[0], usage);
				exit(EXIT_BADARG);
	}

	/* switch over to the script	*/
	if (script) {
		int fd = open(script, 0);
		if (fd < 0) {
			fprintf(stderr, "unable to open script %s: %s\n",
				script, strerror(errno));
			exit(EXIT_BADARG);
		}
		close(0);
		dup(fd);
		close(fd);
	}

	/* create the pipes or open the network connection	*/
	if (port > 0) {
		struct hostent *server;		// DNS server lookup 
		struct sockaddr_in serv_addr;	// server address
		int optval = 1;			// connection options
		int sockfd = socket(AF_INET, SOCK_STREAM, 0);
		setsockopt(sockfd, SOL_SOCKET, SO_REUSEPORT, &optval, sizeof(optval));
		server = gethostbyname(host ? host : "localhost");
		if (server == NULL) {
			fprintf(stderr, "Unable to find address for host %s\n", host);
			exit(EXIT_BADARG);
		}
		bzero((char *) &serv_addr, sizeof(serv_addr));
		serv_addr.sin_family = AF_INET;
		bcopy((char *)server->h_addr,
		      (char *) &serv_addr.sin_addr.s_addr,
		      server->h_length);
		serv_addr.sin_port = htons(port);
		int ret = connect(sockfd, (struct sockaddr*) &serv_addr, sizeof(serv_addr));
		if (ret < 0) {
			fprintf(stderr, "Unable to connect to %s:%d: %s\n",
				host, port, strerror(errno));
		}

		// redirect the pipe file descriptors to the socket
		inPipe[0] = sockfd;
		outPipe[1] = sockfd;
	} else if (pipe(inPipe) >= 0 && pipe(outPipe) >= 0) {
		/* figure out what program we are supposed to start */
		const char *args[MAXARGS];
		int nargs = 0;
		program = argv[argx];
		while( argx < argc && nargs < MAXARGS - 1) {
			args[nargs++] = argv[argx++];
		}
		args[nargs] = 0;

		/* create a new process group, so we can kill it later */
		setpgrp();

		/* kick it off in a sub-process	*/
		child_pid = fork();
		if (child_pid < 0) {
			fprintf(stderr, "Unable to fork child process: %s\n",
				strerror(errno));
			exit(EXIT_NOCHILD);
		} else if (child_pid == 0) {
			if (verbose) {
				fputs("... EXEC ", stderr);
				int i;
				for( i = 0; i < nargs; i++ ) {
					fputs(args[i], stderr);
					fputc(' ', stderr);
				}
				fputc('\n', stderr);
			}
			/* turn pty slave into stdin-stderr for the child	*/
			close(0);
			dup(outPipe[0]);
			close(1);
			dup(inPipe[1]);
			close(2);
			dup(inPipe[1]);

			/* close the original pipe FDs	*/
			close(inPipe[0]);
			close(inPipe[1]);
			close(outPipe[0]);
			close(outPipe[1]);

			/* and kick off the specified program	*/
			int ret = execv(program, (char * const *)args);
			fprintf(stderr, "Unable to exec %s: %s\n",
				program, strerror(errno));
			exit(EXIT_NOCHILD);
		} else {
			close(inPipe[1]);
			close(outPipe[0]);
			if (verbose)
				fprintf(stderr, "... started sub-process %d\n", child_pid);
		}
	} else {
		fprintf(stderr, "unable to create pipe: %s\n",
			strerror(errno));
		exit(EXIT_NOCHILD);
	}

	/* start thread to pass output from pty to stdout	*/
	pthread_t output_thread;
	if (verbose)
		fprintf(stderr, "... starting output thread\n");
	int rc = pthread_create(&output_thread, NULL, process_output, NULL);
	if (rc < 0) {
		fprintf(stderr, "Unable to create output thread: %s\n",
			strerror(errno));
		exit(EXIT_NOCHILD);
	}

	/* prepare for signals		*/
	if (timeout) {
		signal(SIGALRM, time_handler);
		alarm(timeout);
	}
	signal(SIGTERM, handler);
	signal(SIGCHLD, child_handler);

	/* process the commands from stdin to the pty	*/
	char inbuf[BUFSIZE];
	if (verbose)
		fprintf(stderr, "... starting command loop\n");
	while( fgets(inbuf, sizeof inbuf, stdin) != NULL ) {
		/* ignore blank and comment lines */
		const char *s = skipWhite(inbuf);
		if (*s == '#' || *s == '\n' || *s == 0)
			continue;
		if (!process_command(s))
			break;
	}
	if (verbose)
		fprintf(stderr, "... command loop has exited\n");

	/* shut down the read-side of the connection	*/
	close(inPipe[0]);

	/* shut down output thread	*/
	int status;
	writer_stop = 1;
	if (port > 0)
		pthread_cancel(output_thread);
	else {
		pthread_join(output_thread, (void **) &status);
		if (verbose)
			fprintf(stderr, "... output thread exited w/status %d\n", status);
	}
	close(outPipe[1]);

	/* report on child process exit status	*/
	if (port != 0 && child_pid != 0) {
		if (child_status == -1) {
			fprintf(stderr, "!!! CHILD (%d) STATUS NOT ALREADY COLLECTED\n", child_pid);
			waitpid(child_pid, &child_status, 0);
		}
			
		fprintf(stderr, "%s EXIT SIGNAL=%d, STATUS=%d\n", 
			program, child_status & 0xff, child_status >> 8);
#ifdef MAYBE_LATER
		/* kill anything that may still survive under us	*/
		kill(0, SIGTERM);
#endif
	}

	exit(0);
}
