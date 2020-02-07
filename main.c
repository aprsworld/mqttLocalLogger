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
#include <time.h>

static int mqtt_port=1883;
static char mqtt_host[256];
static char logDir[256]="logLocal";
static int unitaryLogDir;
static char logFilePrefix[256];
static char logFileSuffix[256];
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
	

#define ALARM_SECONDS 10
static int run = 1;


void handle_signal(int signum) {
	
	if ( SIGALRM == signum ) {
		/* keep alive modbus connection */

	} else {
		fprintf(stderr,"# %s signal received. Terminating.\n",strsignal(signum));
		run = 0;
		

	}

}

void connect_callback(struct mosquitto *mosq, void *obj, int result) {
	printf("# connect_callback, rc=%d\n", result);
}

void message_callback(struct mosquitto *mosq, void *obj, const struct mosquitto_message *message) {
	char data[4];
	bool match = 0;
	char	_topic[256] = {};
	int	i;

	/* cancel pending alarm */
	alarm(0);
	/* set an alarm to send a SIGALARM if data not received within alarmSeconds */
 	alarm(ALARM_SECONDS);


	printf("got message '%.*s' for topic '%s'\n", message->payloadlen, (char*) message->payload, message->topic);

	/* local copy of our message */
	strncpy(data, (char *) message->payload,3);
	data[3]='\0';

	

	for ( match = i = 0; 0 == match && topicsCount > i; i++ ) 
		mosquitto_topic_matches_sub(topics[i], message->topic, &match);
	strncpy(_topic,topics[i],sizeof(_topic));

	if (match) {
		FILE	*out;
		char	fname[256];
		char	prefix[256] = {};
		char	suffix[256] = {};
		char	timestamp[32] = {};
		struct tm *now;
		time_t right_now;
		fprintf(stderr,"got message for %s\n",message->topic);
		if ( -1 == time(&right_now) ) {
			fprintf(stderr,"# error calling time() %s",strerror(errno));
			exit(1);
		}
		now = localtime(&right_now);
		if ( 0 == now ) {
			fprintf(stderr,"# error calling localtime() %s",strerror(errno));
			exit(1);
		}
		snprintf(timestamp,sizeof(timestamp),"%04d-%02d-%02d %02d:%02d:%02d,",
			1900 + now->tm_year,1 + now->tm_mon, now->tm_mday,now->tm_hour,now->tm_min,now->tm_sec);
			

		if ( ' ' < logFilePrefix[0] )
			strncpy(prefix,logFilePrefix,sizeof(prefix));
		else {
			snprintf(prefix,sizeof(prefix),"%04d%02d%02d",
				1900 + now->tm_year,1 + now->tm_mon, now->tm_mday);
		}
		if ( ' ' < logFileSuffix[0] )
			strncpy(suffix,logFileSuffix,sizeof(suffix));
		snprintf(fname,sizeof(fname),"%s/%s/%s%s",logDir,_topic,prefix,suffix);
		out = fopen(fname,"a");
		if ( 0 == out )	{
			fprintf(stderr,"# error calling fopen(%s) %s\n",fname,strerror(errno));
			exit(1);
		}
		if ( -1 == fputs(timestamp,out)) {
			fprintf(stderr,"# error calling fputs(%s) %s\n",fname,strerror(errno));
			exit(1);
		}

		if ( 1 != fwrite(message->payload,message->payloadlen,1,out )) {
			fprintf(stderr,"# error calling fwrite(%s) %s\n",fname,strerror(errno));
			exit(1);
		}
		if ( 0 != fclose(out)){
			fprintf(stderr,"# error calling fclose(%s) %s\n",fname,strerror(errno));
			exit(1);
		}
	}
}

static int startup_mosquitto(void) {
	char clientid[24];
	struct mosquitto *mosq;
	int rc = 0;

	fprintf(stderr,"# mqtt-modbus start-up\n");

	fprintf(stderr,"# installing signal handlers\n");
	signal(SIGINT, handle_signal);
	signal(SIGTERM, handle_signal);
	signal(SIGALRM, handle_signal);

	fprintf(stderr,"# initializing mosquitto MQTT library\n");
	mosquitto_lib_init();

	memset(clientid, 0, 24);
	snprintf(clientid, 23, "mqtt_modbus_%d", getpid());
	mosq = mosquitto_new(clientid, true, 0);

	if (mosq) {
		int i;
		mosquitto_connect_callback_set(mosq, connect_callback);
		mosquitto_message_callback_set(mosq, message_callback);

		fprintf(stderr,"# connecting to MQTT server %s:%d\n",mqtt_host,mqtt_port);
		rc = mosquitto_connect(mosq, mqtt_host, mqtt_port, 60);

		for ( i = 0; topicsCount > i; i++ )
			mosquitto_subscribe(mosq, NULL, topics[i], 0);

		while (run) {
			rc = mosquitto_loop(mosq, -1, 1);

			if ( run && rc ) {
				printf("connection error!\n");
				sleep(10);
				mosquitto_reconnect(mosq);
			}
		}
		mosquitto_destroy(mosq);
	}

	fprintf(stderr,"# mosquitto_lib_cleanup()\n");
	mosquitto_lib_cleanup();

	return rc;
}


int main(int argc, char **argv) {
	int i,n;
	int rc;

	/* command line arguments */
	while ((n = getopt (argc, argv, "s:p:l:D:dfFt:h")) != -1) {
		switch (n) {
			case 's':	
				strncpy(logFileSuffix,optarg,sizeof(logFileSuffix));
				fprintf(stderr,"# logFileSuffix=%s \n",logFileSuffix);
				break;
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
				fprintf(stdout,"# -s\t\tend each log filename with suffix\n");
				fprintf(stdout,"# -t\t\tmqtt topic must be used once and 1-%d times\n",MAX_TOPICS);
				fprintf(stdout,"# -p\t\tmqtt port\t\tOPTIONAL\n");
				fprintf(stdout,"# -l\t\tlogging derectory, default=logLocal\n");
				fprintf(stdout,"# -1\t\tunitaryLogDir\n");
				fprintf(stdout,"# -D\t\tstart each log filename with prefix rather than YYMMDD\n");
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
	chdir(logDir);
	for ( i = 0; topicsCount > i; i++ )
		if (  _enable_logging_dir(topics[i]))
			return	1;
	rc = startup_mosquitto();
	


	return	rc;
}
