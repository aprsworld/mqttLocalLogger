#define _GNU_SOURCE
#include <stdio.h>
#include <errno.h>
#include <fcntl.h> 
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include <sys/time.h>
#include <stdlib.h>
#include <limits.h>
#include <stdint.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <string.h>
#include <getopt.h>
#include <netinet/in.h>
#include <netdb.h> 
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <json.h>
#include <mosquitto.h>

static int mqtt_port=1883;
static char mqtt_host[256];
static char logDir[256]="logLocal";
static int unitaryLogDir;
static char logFilePrefix[256];
static int splitLogFilesByDay = 1;	
static int flushAfterEachMessage = 1;
#define MAX_TOPICS 16
char *topics[MAX_TOPICS + 1];	// has a sentinel
int topicsCount;

#if 0
static char mqtt_topic[256];
static struct mosquitto *mosq;
#endif

extern char *optarg;
extern int optind, opterr, optopt;

uint64_t microtime() {
	struct timeval time;
	gettimeofday(&time, NULL); 
	return ((uint64_t)time.tv_sec * 1000000) + time.tv_usec;
}

static void signal_handler(int signum) {


	if ( SIGALRM == signum ) {
		fprintf(stderr,"\n# Timeout while waiting for NMEA data.\n");
		fprintf(stderr,"# Terminating.\n");
		exit(100);
	} else if ( SIGPIPE == signum ) {
		fprintf(stderr,"\n# Broken pipe.\n");
		fprintf(stderr,"# Terminating.\n");
		exit(101);
	} else if ( SIGUSR1 == signum ) {
		/* clear signal */
		signal(SIGUSR1, SIG_IGN);

		fprintf(stderr,"# SIGUSR1 triggered data_block dump:\n");
		
		/* re-install alarm handler */
		signal(SIGUSR1, signal_handler);
	} else {
		fprintf(stderr,"\n# Caught unexpected signal %d.\n",signum);
		fprintf(stderr,"# Terminating.\n");
		exit(102);
	}

}
#include <sys/time.h>

static int _enable_logging_dir(const char *s )
{
/* assumes that we are in the correct starting dir */
char	buffer[256+4];
char	path[256+4]  = {};
char	*p,*q = buffer;
struct stat buf;
int rc = 0;
int len;

umask(00);

strncpy(buffer,s,256+3);
len = strlen(buffer);
if ( '/' != buffer[len -1] )	buffer[len] = '/';




while ( p = strsep(&q,"/")) {	// assumes / is the FS separator 
	strcat(path,p);	strcat(path,"/");
	if ( 0 != stat(path,&buf)) {
		/* assume that it does not exist */
		if ( 0 != (rc = mkdir(path,0777)))
			break;
		}
	}
	

if ( 0 != rc ) 
	fprintf(stderr,"# %s %s\n",path,strerror(errno));


return	rc;
}
char	*strsave(char *s )
{
char	*ret_val = 0;

ret_val = malloc(strlen(s)+1);
if ( 0 != ret_val) strcpy(ret_val,s);
return ret_val;	
}
	

int main(int argc, char **argv) {
	int n;

	/* command line arguments */
	while ((n = getopt (argc, argv, "p:l:D:dfFt:h")) != -1) {
		switch (n) {
			case 't':
				if ( topicsCount < MAX_TOPICS )
					topics[topicsCount] = strsave(optarg);
				topicsCount++;	// always count so we can detect too many -t
				break;
			case 'p':
				mqtt_port = atoi(optarg);
				break;
			case 'l':	
				strncpy(logDir,optarg,sizeof(logDir));
				break;
			case '1':
				unitaryLogDir=1;
				fprintf(stderr,"# unitaryLogDir\tlogging to one directory\n");
				break;
			case 'D':	
				strncpy(logFilePrefix,optarg,sizeof(logFilePrefix));
				fprintf(stderr,"# logFilePrefix=%s \n",logFilePrefix);
				splitLogFilesByDay = 0;
				break;
			case 'd':
				splitLogFilesByDay = 1;
				fprintf(stderr,"# logFilePrefix=%s \n","DISABLED");
				break;
			case 'f':
				flushAfterEachMessage = 1;
				fprintf(stderr,"# flushAfterEachMessage=%s \n","ENABLED");
				break;
			case 'F':
				flushAfterEachMessage = 0;
				fprintf(stderr,"# flushAfterEachMessage=%s \n","DISABLED");
				break;
			case 'h':
				fprintf(stdout,"# -t\t\tmqtt topic must be used once and 1-%d times\n",MAX_TOPICS);
				fprintf(stdout,"# -p\t\tmqtt port\t\tOPTIONAL\n");
				fprintf(stdout,"# -l\t\tlogging derectory, default=logLocal\n");
				fprintf(stdout,"# -1\t\tunitaryLogDir\n");
				fprintf(stdout,"# -D\t\tstart each log filename with $prefix rather than YYMMDD\n");
				fprintf(stdout,"# -d\t\tstart each log file with YYMMDD, start new file each day.  (default)\n");
				fprintf(stdout,"# -f\t\tflushAfterEachMessage recieved to log file\n");
				fprintf(stdout,"#\n");
				fprintf(stdout,"# -h\t\tThis help message then exit\n");
				fprintf(stdout,"#\n");
				exit(0);
		}
	}
	 if (optind >= argc) {
               fprintf(stderr, "# Expected mqtt host after options\n");
               exit(EXIT_FAILURE);
	}
	else
		strncpy(mqtt_host,argv[optind],sizeof(mqtt_host));	
	if ( 0 == topicsCount ) {
		fprintf(stderr,"# There must be at least one -t and no more than %d -t\n",MAX_TOPICS);
               exit(EXIT_FAILURE);
	}

	if ( MAX_TOPICS < topicsCount ) {
		fprintf(stderr,"# %d MAX_TOPICS < %d topicsCount\n",MAX_TOPICS,topicsCount);
               exit(EXIT_FAILURE);
	}


		

	/* install signal handler */
	signal(SIGALRM, signal_handler); /* timeout */
	signal(SIGUSR1, signal_handler); /* user signal to do data block debug dump */
	signal(SIGPIPE, signal_handler); /* broken TCP connection */

	if ( _enable_logging_dir(logDir)) {
		return	1;
	}


	return	0;
}
