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
#include <mosquitto.h>
#include <time.h>
#include <ctype.h>
#include <math.h>

static int mqtt_port=1883;
static char mqtt_host[256];
char mqtt_topic[256];
char mqtt_meta_topic[256];
char input_file_name[256];
static char *mqtt_user_name,*mqtt_passwd;
static struct mosquitto *mosq;
static int disable_mqtt_output;
static int quiet_flag;

static void _mosquitto_shutdown(void);



extern char *optarg;
extern int optind, opterr, optopt;

int outputDebug=0;
int milliseconds_timeout=500;
int alarmSeconds=5;
int no_meta;
int retainedFlag;

FILE *stdout;
FILE *stderr;


uint64_t microtime() {
	struct timeval time;
	gettimeofday(&time, NULL); 
	return ((uint64_t)time.tv_sec * 1000000) + time.tv_usec;
}

void connect_callback(struct mosquitto *mosq, void *obj, int result) {
	if ( 5 == result ) {
		fprintf(stderr,"# --mqtt-user-name and --mqtt-passwd required at this site.\n");
	}
	fprintf(stderr,"# connect_callback, rc=%d\n", result);
}

static void signal_handler(int signum) {


	if ( SIGALRM == signum ) {
		fprintf(stderr,"\n# Timeout while waiting for NMEA data.\n");
		fprintf(stderr,"# Terminating.\n");
		_mosquitto_shutdown();
		exit(100);
	} else if ( SIGPIPE == signum ) {
		fprintf(stderr,"\n# Broken pipe.\n");
		fprintf(stderr,"# Terminating.\n");
		_mosquitto_shutdown();
		exit(101);
	} else if ( SIGUSR1 == signum ) {
		/* clear signal */
		signal(SIGUSR1, SIG_IGN);

		fprintf(stderr,"# SIGUSR1 triggered data_block dump:\n");
		
		/* re-install alarm handler */
		signal(SIGUSR1, signal_handler);
	} else if ( SIGTERM == signum ) {
		fprintf(stderr,"# Terminating.\n");
		_mosquitto_shutdown();
		exit(0);
	} else {
		fprintf(stderr,"\n# Caught unexpected signal %d.\n",signum);
		fprintf(stderr,"# Terminating.\n");
		_mosquitto_shutdown();
		exit(102);
	}

}
#include <sys/time.h>

char	*strsave(const char *s )
{
	char	*ret_val = 0;

	ret_val = malloc(strlen(s)+1);
	if ( 0 != ret_val) {
	       	strcpy(ret_val,s); 
	}
	return ret_val;	
}
	
static struct mosquitto * _mosquitto_startup(void) {
	char clientid[24];
	int rc = 0;


	fprintf(stderr,"# initializing mosquitto MQTT library\n");
	mosquitto_lib_init();

	memset(clientid, 0, 24);
	snprintf(clientid, 23, "mqtt-send-example_%d", getpid());
	mosq = mosquitto_new(clientid, true, 0);

	if (mosq) {
		if ( 0 != mosq,mqtt_user_name && 0 != mqtt_passwd ) {
			mosquitto_username_pw_set(mosq,mqtt_user_name,mqtt_passwd);
		}
		mosquitto_connect_callback_set(mosq, connect_callback);

		fprintf(stderr,"# connecting to MQTT server %s:%d\n",mqtt_host,mqtt_port);
		rc = mosquitto_connect(mosq, mqtt_host, mqtt_port, 60);

		/* start mosquitto network handling loop */
		mosquitto_loop_start(mosq);
		}

return	mosq;
}

static void _mosquitto_shutdown(void) {

	if ( mosq ) {
		
		/* disconnect mosquitto so we can be done */
		mosquitto_disconnect(mosq);
		/* stop mosquitto network handling loop */
		mosquitto_loop_stop(mosq,0);


		mosquitto_destroy(mosq);
		}

	fprintf(stderr,"# mosquitto_lib_cleanup()\n");
	mosquitto_lib_cleanup();
}

int feedValuesToMQTT_pub(const char *message, const char *topic  ) {
	int rc = 0;
	if ( 0 == quiet_flag ) {
		fputs(message,stdout);
		fflush(stdout);
	}
	
	if ( 0 == disable_mqtt_output ) {

		static int messageID;
		/* instance, message ID pointer, topic, data length, data, qos, retain */
		rc = mosquitto_publish(mosq, &messageID, topic, strlen(message), message, 2, retainedFlag ); 
		retainedFlag = 0; /* default is off */


		if (0 != outputDebug) fprintf(stderr,"# mosquitto_publish provided messageID=%d and return code=%d\n",messageID,rc);

		/* check return status of mosquitto_publish */ 
		/* this really just checks if mosquitto library accepted the message. Not that it was actually send on the network */
		if ( MOSQ_ERR_SUCCESS == rc ) {
			/* successful send */
		} else if ( MOSQ_ERR_INVAL == rc ) {
			fprintf(stderr,"# mosquitto error invalid parameters\n");
		} else if ( MOSQ_ERR_NOMEM == rc ) {
			fprintf(stderr,"# mosquitto error out of memory\n");
		} else if ( MOSQ_ERR_NO_CONN == rc ) {
			fprintf(stderr,"# mosquitto error no connection\n");
		} else if ( MOSQ_ERR_PROTOCOL == rc ) {
			fprintf(stderr,"# mosquitto error protocol\n");
		} else if ( MOSQ_ERR_PAYLOAD_SIZE == rc ) {
			fprintf(stderr,"# mosquitto error payload too large\n");
		} else if ( MOSQ_ERR_MALFORMED_UTF8 == rc ) {
			fprintf(stderr,"# mosquitto error topic is not valid UTF-8\n");
		} else {
			fprintf(stderr,"# mosquitto unknown error = %d\n",rc);
		}
	}


	return	rc;
}
static void process_file(void) {
	double d;
	struct timeval time;
	char	buffer[256] = {};
	FILE *in = fopen(input_file_name,"r");
	if ( 0 == in ) {
		fprintf(stderr,"# cannot open %s for reading.\n",input_file_name);
		return;
	}
	do {
		usleep(10);
		gettimeofday(&time, NULL); 
	} while ( time.tv_usec > 100 ) ;	/* wait to top of the second */

	while (1) {
		if ( 0 == fgets(buffer,sizeof(buffer),in)) {
			break;
		}
		if ( 0 != outputDebug ) {
			fprintf(stderr,"# %s",buffer);
		}
		
		d = atof(buffer);
		snprintf(buffer,sizeof(buffer),"{ \"value\": %lf }",d);
		feedValuesToMQTT_pub(buffer,mqtt_topic);
		usleep(1000000/40);	/* denom == hertz */
	}
	fclose(in);
}
enum arguments {
	A_disable_mqtt = 512,
	A_mqtt_host,
	A_mqtt_topic,
	A_mqtt_meta_topic,
	A_mqtt_port,
	A_mqtt_user_name,
	A_mqtt_password,
	A_input_file,
	A_quiet,
	A_verbose,
	A_help,
};



int main(int argc, char **argv) {
	int n;


	while (1) {
		// int this_option_optind = optind ? optind : 1;
		int option_index = 0;
		static struct option long_options[] = {
			/* normal program */
		        {"input-file",                       1,                 0, A_input_file },
		        {"mqtt-host",                        1,                 0, A_mqtt_host },
		        {"mqtt-topic",                       1,                 0, A_mqtt_topic },
		        {"mqtt-port",                        1,                 0, A_mqtt_port },
		        {"mqtt-user-name",                   1,                 0, A_mqtt_user_name },
		        {"mqtt-passwd",                      1,                 0, A_mqtt_password },
			{"disable-mqtt",                     no_argument,       0, A_disable_mqtt },
			{"quiet",                            no_argument,       0, A_quiet, },
			{"verbose",                          no_argument,       0, A_verbose, },
		        {"help",                             no_argument,       0, A_help, },
			{},
		};

		n = getopt_long(argc, argv, "", long_options, &option_index);

		if (n == -1) {
			break;
		}
		

	/* command line arguments */
		switch (n) {
			case A_input_file:
				strncpy(input_file_name,optarg,sizeof(input_file_name));
				break;
			case A_disable_mqtt:
				disable_mqtt_output = 1;
				break;
			case A_mqtt_topic:	
				strncpy(mqtt_topic,optarg,sizeof(mqtt_topic));
				break;
			case A_mqtt_meta_topic:	
				strncpy(mqtt_meta_topic,optarg,sizeof(mqtt_meta_topic));
				break;
			case A_mqtt_host:	
				strncpy(mqtt_host,optarg,sizeof(mqtt_host));
				break;
			case A_mqtt_port:
				mqtt_port = atoi(optarg);
				break;
			case A_mqtt_user_name:
				mqtt_user_name = strsave(optarg);
				break;
			case A_mqtt_password:
				mqtt_passwd = strsave(optarg);
				break;
			case A_verbose:
				outputDebug=1;
				fprintf(stderr,"# verbose (debugging) output to stderr enabled\n");
				break;
			case A_quiet:
				quiet_flag = 1;
				fprintf(stderr,"# quiet no output packets to standard out\n");
				break;
			case A_help:
				fprintf(stdout,"# --disable-mqtt\t\tdisable mqtt output\n");
				fprintf(stdout,"# --mqtt-topic\t\t\tmqtt topic\n");
				fprintf(stdout,"# --mqtt-meta-topic\t\t\tmqtt meta topic\n");
				fprintf(stdout,"# --mqtt-host\t\t\tmqtt host\n");
				fprintf(stdout,"# --mqtt-port\t\t\tmqtt port(optional)\n");
				fprintf(stdout,"# --mqtt-user-name\t\tmaybe required depending on system\n");
				fprintf(stdout,"# --mqtt-passwd\t\t\tmaybe required depending on system\n");
				fprintf(stdout,"# --verbose\t\t\tOutput verbose / debugging to stderr\n");
				fprintf(stdout,"#\n");
				fprintf(stdout,"# --help\t\t\tThis help message then exit\n");
				fprintf(stdout,"#\n");
				exit(0);
		}
	}
	if ( ' ' > input_file_name[0] ) {
		fputs("# <--input-file> is requied\n",stderr); 
		exit(1); 

	}
	if ( 0 == disable_mqtt_output && ' ' >= mqtt_host[0] ) { 
		fputs("# <--mqtt-host is requied when outputting to mqtt>\n",stderr); 
		exit(1); 
	} else {
		fprintf(stderr,"# --mqtt_host=%s\n",mqtt_host);
	}
	if ( 0 == disable_mqtt_output && ' ' >= mqtt_topic[0] ) { 
		fputs("# <--mqtt_topic> is required  when outputting to mqtt\n",stderr); 
		exit(1); 
	} else {
		fprintf(stderr,"# --mqtt_topic=%s\n",mqtt_topic);
	}

	if ( 0 == disable_mqtt_output && 0 == _mosquitto_startup() ) {
		return	1;
	}


	/* install signal handler */
	signal(SIGALRM, signal_handler); /* timeout */
	signal(SIGUSR1, signal_handler); /* user signal to do data block debug dump */
	signal(SIGTERM, signal_handler); /* user signal to terminate */
	signal(SIGPIPE, signal_handler); /* broken TCP connection */

	process_file();

	sleep(1);	


	if ( 0 == disable_mqtt_output ) {
		_mosquitto_shutdown();
	}


	return	0;
}
