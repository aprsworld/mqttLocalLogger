
#define _GNU_SOURCE
#include <stdio.h>
#include <ctype.h>
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
#include <json_tokener.h>
#include <json_pointer.h>
#include <mosquitto.h>
#include <time.h>
#include <math.h>
#include "queue.h"

/* --------------------------------------------------------------------------
 *   This program will not do display.   The program is designed to run a
 *   number of seconds after real this to ensure that the latency of 
 *   mqtt system will not grealy affect the output of the csv file.
 * ----------------------------------------------------------------------- */

#define ALARM_SECONDS 600
static int _columnDebugCount;
static int run = 1;
static int mqtt_port=1883;
static char mqtt_host[256];
static char configuration[256];
static char logDir[256]="logLocal";
static char *mqtt_user_name,*mqtt_passwd;
static int unitaryLogFile;
static char logFilePrefix[256];
static char logFileSuffix[256] = ".csv";
const char EOF_DATE[] = "~~~~~~~~~~~~";	/* strcmp to 3030-12-25 EOF_DATE will always be bigger */
const char BEG_DATE[] = "2000-01-01 00:00:00.000";
int outputDebug=0;
int outputSeparatorCount;
int hertz = 0;
int milliseconds;
int noOutputStdout =0;
static int splitLogFilesByDay = 1;	

char last_packet_received[256];
extern char *optarg;
extern int optind, opterr, optopt;

static int _outputHeaders(FILE *out );

char	*strsave(char *s ) {
	char	*ret_val = 0;

	ret_val = malloc(strlen(s)+1);
	if ( 0 != ret_val) {
		strcpy(ret_val,s);
	}
	return ret_val;	
}
typedef struct topics {
	struct topics *left,*right;
	char	*topic;
	char *t_packet;
	int packetCount;
	json_object *t_jobj;  /* packet for last second */
	json_object *new_jobj;	/* packet for current second */
	Queue_t t_q;
	char *t_date;
	int randomizer;
	}	TOPICS;

TOPICS *topic_root = 0;
#define MAX_TOPICS 128
TOPICS *topics_heap[MAX_TOPICS];
int topics_heapCount;

char *initialize_topic_heap(TOPICS *p ) {
	topics_heap[topics_heapCount] = p;
	topics_heapCount++;
	
	return strsave((char *) BEG_DATE);
}

TOPICS * add_topic(char *s ) {
	TOPICS *p,*q;
	int	cond;
	int	flag;

	if ( 0 == topic_root ) {
		topic_root = calloc(sizeof(TOPICS),1);
		topic_root->topic = strsave(s);
		topic_root->t_date = initialize_topic_heap(topic_root);
		return topic_root;
	}
	p = topic_root;
	for ( ;0 != p; ) {
		cond = strcmp(p->topic,s);
		q = p;
		if ( 0 == cond )	return	p;	// no reason to re-subscribe
		if ( 0 < cond ) {
			p = q->left;	flag = 1;
		}
		else {
			p = q->right;	flag = -1;
		}
	}
	/* if here then it is a new topic */
	p = calloc(sizeof(TOPICS),1);
	p->topic = strsave(s);
	if ( 1 == flag )
		q->left = p;
	else
		q->right = p;

	p->t_date = initialize_topic_heap(p);

	return	p;
}
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




while ( 0 != (p = strsep(&q,"/"))) {	// assumes / is the FS separator 
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

int timestamp_to_seconds(char *s ) {
	/* expecting format yyyy-mm-dd hh:mm:ss */
	char *p,*q;
	char buffer[64];
	struct tm now = {};
	strncpy(buffer,s,sizeof(buffer));
	q = buffer;
	p = strsep(&q,"-");
	if ( 0 == p ) {
		goto parse_failed;
	}
	now.tm_year = atoi(p) - 1900;
	p = strsep(&q,"-");
	if ( 0 == p ) {
		goto parse_failed;
	}
	now.tm_mon = atoi(p) - 1;
	p = strsep(&q," ");
	if ( 0 == p ) {
		goto parse_failed;
	}
	now.tm_mday = atoi(p);
	/* date is parses */

	p = strsep(&q,":");
	if ( 0 == p ) {
		goto parse_failed;
	}
	now.tm_hour = atoi(p);
	p = strsep(&q,":");
	if ( 0 == p ) {
		goto parse_failed;
	}
	now.tm_min = atoi(p);
	now.tm_sec = atoi(q);

	return mktime(&now);

parse_failed:
	return -1;
}
static FILE * _openLogfile(char *last_date) {
	FILE	*out;
	char	fname[256+256+256+4];
	char	prefix[256] = {};
	char	suffix[256] = {};
	char	path[256+256+256+4] = {};
	struct tm *now;
	struct timeval time;
	time_t x = timestamp_to_seconds(last_date);
	int exists_flag = 0;
	struct stat buf;
        gettimeofday(&time, NULL);


	time.tv_sec = ( ( 1 + x ) != time.tv_sec ) ? time.tv_sec : x; /* because log is not written in real time */

	now = localtime(&time.tv_sec);
	if ( 0 == now ) {
		fprintf(stderr,"# error calling localtime() %s",strerror(errno));
		exit(1);
	}

	if ( ' ' < logFilePrefix[0] )
		strncpy(prefix,logFilePrefix,sizeof(prefix));
	else {
		snprintf(prefix,sizeof(prefix),"%04d%02d%02d",
			1900 + now->tm_year,1 + now->tm_mon, now->tm_mday);
	}
	if ( ' ' < logFileSuffix[0] )
		strncpy(suffix,logFileSuffix,sizeof(suffix));
	if ( 0 == unitaryLogFile ) {
		snprintf(path,sizeof(path),"%s/",logDir);
		if ( _enable_logging_dir(path)) {
			exit(1);
		}
		snprintf(fname,sizeof(fname),"%s%s",prefix,suffix);
	}
	else snprintf(fname,sizeof(fname),"%s%s",prefix,suffix);
		
	exists_flag = ( 0 == stat(fname,&buf));
	out = fopen(fname,"a");
	if ( 0 == out )	{
		fprintf(stderr,"# error calling fopen(%s) %s\n",fname,strerror(errno));
		exit(1);
	}
	if ( 0 == exists_flag ) {
		(void) _outputHeaders(out);
	}
	return	out;
}


uint64_t microtime() {
	struct timeval time;
	gettimeofday(&time, NULL); 
	return ((uint64_t)time.tv_sec * 1000000) + time.tv_usec;
}

enum Outputs {
	/* 1-99 are operations that do not require building a json_object from the packet */
	value = 1,	/* this is only formatable as a string */

	/* 100 and above required  require building a json_object from the packet */
	count = 100,
	value_integer,
	value_double,
	mean,
	sum,
	standard_deviation,
	maximum,
	minimum,
};
typedef struct {
	enum Outputs value;
	char *key;
} OutputTypes;

OutputTypes OutputTypesTable[] = {
	{value,"value",},
	{count,"count",},
	{value_integer,"value_integer",},
	{value_double,"value_double",},
	{mean,"mean",},
	{sum,"sum",},
	{standard_deviation,"standard_deviation",},
	{maximum,"maximum",},
	{minimum,"minimum",},
	{0,0,},
};

enum Outputs get_csvOutputType(char *s ) {
	OutputTypes *p = OutputTypesTable;

	for ( ; 0 != p->key && 0 != strcmp(s,p->key); p++ ) {
		}
	
	if ( 0 == p->key ) {
		fprintf(stderr,"# cannot find csvOutputType %s\n",s);
	}
	return	p->value;
}
typedef struct {
	int count;
	double sum;
	double maximum,minimum;
	/* internal standard_deviation stuff */
	double m,s;	/* m is the running mean and s is the running standard_deviation */
	int n;	/* m, s, and n must be zeroed after each output */
} STATISTICS;


typedef struct {
	char *csvColumn;
	char *csvOutput;
	char *csvTitle;
	char *mqttTopic;
	char *jsonPath;
	char *csvOutputFormat;	/* optional used by snprintf */
	int debug;
	/* internal elements */
	STATISTICS periodic, continuous;
	int integerColumn;
	TOPICS *c_this_topic;
	enum Outputs csvOutputType;
	uint64_t	uLastUpdate;	/* the microtime of the last packet */
	void *c_packet;
	int packet_count;
} COLUMN;

COLUMN columns[256];
int columnsCount;

json_object *parse_a_string(char *string ) {

	uint64_t latency = (uint64_t) 0 - microtime();
	json_object *jobj = NULL;
	json_tokener *tok = json_tokener_new();
	const char *mystring = string;
	int stringlen = 0;
	enum json_tokener_error jerr;

	stringlen = strlen(mystring);
	jobj = json_tokener_parse_ex(tok, mystring, stringlen);
	jerr = json_tokener_get_error(tok);

	if (jerr != json_tokener_success) {
		fprintf(stderr, "Error: %s\n", json_tokener_error_desc(jerr));
		exit(1);
	}
	if (tok->char_offset < stringlen) {
		// Handle extra characters after parsed object as desired.
		// e.g. issue an error, parse another object from that point, etc...
	}
	if ( outputDebug ) {
		fprintf(stderr,"# parse_a_string() output = %s\n",json_object_to_json_string_ext(jobj, JSON_C_TO_STRING_PRETTY));
		latency += microtime();
		fprintf(stderr,"# parse_a_string() latency %lld\n",(long long int)latency);
	}
	json_tokener_free(tok);

	return	jobj;
}
void updateColumnStats( COLUMN *this_column,  TOPICS *this_topic ) {
	this_column->uLastUpdate = microtime();
	if ( count > this_column->csvOutputType ) {
		return;	/* no stats for this column */
	}
	/* we now must build a json_object */
	this_column->c_this_topic->t_jobj = parse_a_string(this_column->c_this_topic->t_packet);
	int rc = -1;
	json_object *tmp = NULL;
	if ( 0 !=  this_column->c_this_topic->t_jobj) {
		rc = json_pointer_get(this_column->c_this_topic->t_jobj,this_column->jsonPath,&tmp);
	}
	if ( 0 == rc ) { /* this element exist  so we can stat this element */
		double x,previous_mean;
	
		switch ( this_column->csvOutputType ) {
			case	count:
				this_column->periodic.count++;
				this_column->continuous.count++;
				break;
			case	mean:
				this_column->periodic.count++;
				this_column->continuous.count++;
				x = atof(json_object_to_json_string_ext(tmp, JSON_C_TO_STRING_PRETTY));
				this_column->periodic.sum += x;
				this_column->continuous.sum += x;
				break;
			case	sum:
				x = atof(json_object_to_json_string_ext(tmp, JSON_C_TO_STRING_PRETTY));
				this_column->periodic.sum += x;
				this_column->continuous.sum += x;
				break;
			case	standard_deviation:
				x = atof(json_object_to_json_string_ext(tmp, JSON_C_TO_STRING_PRETTY));
				previous_mean = this_column->periodic.m;
				this_column->periodic.n++;
				this_column->periodic.m = ( x - this_column->periodic.m) /this_column->periodic.n;
				this_column->periodic.s += ( x - this_column->periodic.m) * ( x - previous_mean);

				previous_mean = this_column->continuous.m;
				this_column->continuous.n++;
				this_column->continuous.m = ( x - this_column->continuous.m) /this_column->continuous.n;
				this_column->continuous.s += ( x - this_column->continuous.m) * ( x - previous_mean);
				break;
			case	maximum:
				x = atof(json_object_to_json_string_ext(tmp, JSON_C_TO_STRING_PRETTY));
				this_column->periodic.maximum = 
					( x > this_column->periodic.maximum) ? x : this_column->periodic.maximum;
				this_column->continuous.maximum = 
					( x > this_column->continuous.maximum) ? x : this_column->continuous.maximum;
				break;
			case	minimum:
				x = atof(json_object_to_json_string_ext(tmp, JSON_C_TO_STRING_PRETTY));
				this_column->periodic.minimum = 
					( x < this_column->periodic.minimum) ? x : this_column->periodic.minimum;
				this_column->continuous.minimum = 
					( x < this_column->continuous.minimum) ? x : this_column->continuous.minimum;
				break;
			default:
				break;
				
		}
		if ( 0 != this_column->c_this_topic->t_jobj ) {
			json_object_put(this_column->c_this_topic->t_jobj);
		}
	}

}
void update_this_Topic( int i, char * packet, int packetlen ) {

	TOPICS *this_topic = columns[i].c_this_topic;

	if ( 0 == this_topic ) {
		fprintf(stderr,"# internal error findTopicColumns\n");
		exit(1);
	}
	if ( 0 != this_topic->t_packet ) {
		free(this_topic->t_packet);
		this_topic->t_packet = 0;
	}
	char *tmp = calloc(1,packetlen + 1);
	if ( 0 != tmp ) {
		memcpy(tmp,packet,packetlen);
	}
	
	this_topic->t_packet = tmp;
	this_topic->packetCount++;;
}
#if 0
int findTopicColumns(char *s, char *packet, int packetlen ) {
	int i = 0;
	int rc = true;
	if ( outputDebug ) {
		fprintf(stderr,"# findTopicColumns '%s'\n",packet);
	}
	for ( i = 0; columnsCount > i; i++ ) {
		if ( 0 == strcmp(columns[i].mqttTopic,s) ) {
			update_this_Topic( i , packet , packetlen );
			if ( 0 != columns[i].debug ) {
				_columnDebugCount++;
			}
			if ( outputDebug && columns[i].debug ) {
				fprintf(stderr,"# %d packet_count \n",
					columns[i].packet_count);
			}
			if ( 0 != columns[i].c_packet ) {
				free ( columns[i].c_packet );
			}
			char *tmp2 = calloc(1,packetlen + 1);
			if ( 0 != tmp2 ) {
				memcpy(tmp2,packet,packetlen);
			}
			columns[i].c_packet = tmp2;	/* this is persistent so we can display it */
			columns[i].packet_count++;
			TOPICS *this_topic = columns[i].c_this_topic;
			updateColumnStats( columns + i,this_topic);
			rc = false;
		}
	}
		
	return	rc;
}
#endif
	

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

void connect_callback(struct mosquitto *mosq, void *obj, int result) {
	if ( 5 == result ) {
		fprintf(stderr,"# --mqtt-user-name and --mqtt-passwd required at this site.\n");
	}
	if ( outputDebug ) {
		fprintf(stderr,"# connect_callback, rc=%d\n", result);
	}
}

TOPICS * findTOPIC( char *s ) {
	TOPICS *p,*q;
	int	cond;

	p = topic_root;
	for ( ;0 != p; ) {
		cond = strcmp(p->topic,s);
		q = p;
		if ( 0 == cond )	return	p;
		if ( 0 < cond ) {
			p = q->left;
		}
		else {
			p = q->right;
		}
	}
	return	0;
}




static void column_clear(void) {
	int i;
	for ( i = 0; columnsCount > i ; i++ ) {
		COLUMN * t = columns + i;
		t->periodic.n = t->periodic.count = 0;
		t->periodic.s = t->periodic.m = t->periodic.sum = 0.0;
		t->periodic.maximum = 0.0 - INFINITY;
		t->periodic.minimum = INFINITY;
	}
}	

void output_clear(TOPICS *p) {
	if ( 0 == p ) {
		return;
	}
	output_clear(p->left);
	if ( 0 != p->t_packet ) {
		free(p->t_packet);
		p->t_packet = 0;
	}
	if ( 0 != p->t_jobj ) {
		json_object_put(p->t_jobj);
		p->t_jobj = NULL;
	}
	output_clear(p->right);
}
void clear_all_outputs(void) {
	output_clear(topic_root);
	column_clear();
}
void topics_mosquitto_subscribe(TOPICS *p, struct mosquitto *mosq) {
	if ( 0 == p ){
		return;
	}
	topics_mosquitto_subscribe(p->left,mosq);
	if ( 0 != p->topic[0] ) {
		mosquitto_subscribe(mosq, NULL, p->topic, 2);
	}
	topics_mosquitto_subscribe(p->right,mosq);
}
struct sigaction new_action, old_action;
COLUMN fault_column;

void sigsegv_handler(int sig, siginfo_t *si, void *unused) {

	if ( 0 != fault_column.csvOutputFormat ) {
		fprintf(stderr,"# Got SIGSEGV applying \"%s\"\n",fault_column.csvOutputFormat);
		fprintf(stderr,"# to Column \"%s\"\n",fault_column.csvColumn);
	}
	fprintf(stderr,"# Got SIGSEGV at address: 0x%lx\n",
		(long) si->si_addr);
	exit(EXIT_FAILURE);
}
void set_sigsegv(COLUMN thisColumn ) {
	fault_column = thisColumn;

	/* Set up the structure to specify the new action. */
	new_action.sa_sigaction = sigsegv_handler;
	sigemptyset (&new_action.sa_mask);
	new_action.sa_flags = 0;

	sigaction (SIGSEGV, NULL, &old_action);
	sigaction (SIGSEGV, &new_action, NULL);
}
void unset_sigsegv( void ) {
	sigaction (SIGSEGV, &old_action, NULL);
}
void outputThisJson( json_object *jobj, COLUMN thisColumn, FILE *out, int displayFlag ) {
	char buffer[256] = {};
	char *fmt = (char *)thisColumn.csvOutputFormat;
	STATISTICS t = (0 == displayFlag) ? thisColumn.periodic : thisColumn.continuous;

	if ( 0 != thisColumn.debug ) {
		_columnDebugCount++;
	}

	set_sigsegv(thisColumn);
	const char *tmp = json_object_to_json_string_ext(jobj, JSON_C_TO_STRING_PRETTY);
	int nullFlag = ( 0 == strcmp(tmp,"null"));
	
	switch ( thisColumn.csvOutputType ) {	
		case value:
			if ( 0 == nullFlag ) {
				fmt = ( 0 != fmt ) ? fmt : "%s";
				snprintf(buffer,sizeof(buffer),fmt, tmp);
			}
			break;
		case value_integer:
			if ( 0 == nullFlag ) {
				fmt = ( 0 != fmt ) ? fmt : "%d";
				snprintf(buffer,sizeof(buffer),fmt, atoi(tmp));
			}
			break;
		case value_double:
			if ( 0 == nullFlag ) {
				fmt = ( 0 != fmt ) ? fmt : "%lf";
				snprintf(buffer,sizeof(buffer),fmt, atof(tmp));
			}
			break;
		case count:
			fmt = ( 0 != fmt ) ? fmt : "%d";
			snprintf(buffer,sizeof(buffer),fmt,t.count);
			break;
		case mean:
			fmt = ( 0 != fmt ) ? fmt : "%lf";
			snprintf(buffer,sizeof(buffer),fmt,t.sum/t.count);
			break;
		case sum:
			fmt = ( 0 != fmt ) ? fmt : "%lf";
			snprintf(buffer,sizeof(buffer),fmt,t.sum);
			break;
		case standard_deviation:
			fmt = ( 0 != fmt ) ? fmt : "%lf";
			snprintf(buffer,sizeof(buffer),fmt,( 0 == t.n) ? NAN :
				sqrt((t.s/t.n)));
			break;
		case maximum:
			fmt = ( 0 != fmt ) ? fmt : "%lf";
			if ( ( 0.0 - INFINITY) != t.maximum ) {
				snprintf(buffer,sizeof(buffer),fmt,t.maximum);
			} else {
				strncpy(buffer,"NULL",sizeof(buffer));
			}
			break;
		case minimum:
			fmt = ( 0 != fmt ) ? fmt : "%lf";
			if ( INFINITY != t.minimum ) {
				snprintf(buffer,sizeof(buffer),fmt,t.minimum);
			} else {
				strncpy(buffer,"NULL",sizeof(buffer));
			}
			break;
			
	}
	unset_sigsegv();
	
	if ( 0 == noOutputStdout ) {
		fprintf(stdout,"%s,",buffer);
	}
	if ( 0 != out ) {
		fprintf(out,"%s,",buffer);
	}
	
}
#if 0
int outputThisColumn( int idx, FILE *out ) {
	COLUMN thisColumn = columns[idx];

	if ( 0 == thisColumn.csvColumn[0] ) {
		return	0;
	}

	/* now grab the data for this column */
	json_object *tmp = NULL;
	int rc = -1;
	if ( 0 != thisColumn.c_this_topic->t_jobj ) {
		rc = json_pointer_get(thisColumn.c_this_topic->t_jobj,thisColumn.jsonPath,&tmp);
		if ( 0 != rc ) {
			fprintf(stderr,"# rc = %d\n",rc);
		}
	}

	int i = outputSeparatorCount + 1;
	for ( ; thisColumn.integerColumn > i; i++ ) {
		if ( 0 != out ) {
			fprintf(out,",");
		}
	}
		
	

	outputThisJson(tmp,thisColumn,out,0);
	outputSeparatorCount = thisColumn.integerColumn;
	
	return	0;
}
#endif
#if 1
int outputThisColumn( int idx, FILE *out ) {
	COLUMN thisColumn = columns[idx];

	if ( 0 != thisColumn.debug ) {
		_columnDebugCount++;
	}
	if ( 0 == thisColumn.csvColumn[0] ) {
		return	0;
	}

#if 0
	if ( 0 != thisColumn.c_this_topic->t_jobj ) {
		json_object_put(thisColumn.c_this_topic->t_jobj );
	}
	thisColumn.c_this_topic->t_jobj = parse_a_string(thisColumn.c_this_topic->t_packet);
#endif

	/* now grab the data for this column */
	json_object *tmp = NULL;
	int rc = -1;
	if ( 0 != thisColumn.c_this_topic->t_jobj ) {
		rc = json_pointer_get(thisColumn.c_this_topic->t_jobj,thisColumn.jsonPath,&tmp);
		if ( 0 != rc ) {
			fprintf(stderr,"# rc = %d\n",rc);
		}
	}

	int i = outputSeparatorCount + 1;
	for ( ; thisColumn.integerColumn > i; i++ ) {
		if ( 0 == noOutputStdout ) {
			fprintf(stdout,",");
		}
		if ( 0 != out ) {
			fprintf(out,",");
		}
	}
		
	

	outputThisJson(tmp,thisColumn,out,0);
	outputSeparatorCount = thisColumn.integerColumn;
#if 0
	/* tmp does not have to be released because it points into thisColumn.this_topic->jobj */
	if ( 0 != thisColumn.c_this_topic->t_jobj ) {
		json_object_put(thisColumn.c_this_topic->t_jobj );
		thisColumn.c_this_topic->t_jobj = 0;
	}
#endif
	
return	0;
}
#endif
int next_hertz(struct timeval *real_time, struct timeval *trigger_time ) {
	static uint64_t hertz_interval ;

	if ( 0 == hertz_interval ) {
		hertz_interval =  ((uint64_t)1000000/hertz);
	}


	if ( 0  == trigger_time->tv_sec ) {
		trigger_time->tv_sec = real_time->tv_sec +1;	/* start at the next second */
		trigger_time->tv_usec = 0;
		return true;
	}
	
	uint64_t t1,t2;
	t1 = ((uint64_t)real_time->tv_sec * 1000000) + real_time->tv_usec;
	t2 = ((uint64_t)trigger_time->tv_sec * 1000000) + trigger_time->tv_usec;
	if ( t1 < t2 ) {
		return	true;
	}
	/*   we are triggered */
	if ( 1 == hertz ) {
		while ( t1 > t2 ) {
			trigger_time->tv_sec++;
			t2 = ((uint64_t)trigger_time->tv_sec * 1000000) + trigger_time->tv_usec;
		}
	}
	else {
		t2 += hertz_interval;
		trigger_time->tv_sec = t2 / 1000000;
		trigger_time->tv_usec = t2 % 1000000;
	}
		
	return	false;	/* we have been triggered. */	

}
int next_msec(struct timeval *real_time, struct timeval *trigger_time ) {

	uint64_t latency = (uint64_t) 0 - microtime();

	if ( 0  == trigger_time->tv_sec ) {
		trigger_time->tv_sec = real_time->tv_sec +1;	/* start at the next second */
		trigger_time->tv_usec = 0;
		return true;
	}
	
	uint64_t t1,t2;
	t1 = ((uint64_t)real_time->tv_sec * 1000000) + real_time->tv_usec;
	t2 = ((uint64_t)trigger_time->tv_sec * 1000000) + trigger_time->tv_usec;
	if ( t1 < t2 ) {
		return	true;
	}
	/*   we are triggered */
	t2 += milliseconds * 1000;
	trigger_time->tv_sec = t2 / 1000000;
	trigger_time->tv_usec = t2 % 1000000;


	if ( outputDebug ) {
		latency += microtime();
		fprintf(stderr,"# next_msec latency %lld\n",(long long int)latency);
	}
			
		
	return	false;	/* we have been triggered. */	

}
int outputThisColumnHeader( int idx, FILE *out ) {
	COLUMN thisColumn = columns[idx];

	if ( 0 == thisColumn.csvColumn[0] ) {
		return	0;
	}


	int i = outputSeparatorCount + 1;
	for ( ; thisColumn.integerColumn > i; i++ ) {
		if ( 0 == noOutputStdout ) {
			fprintf(stdout,",");
		}
		if ( 0 != out ) {
			fprintf(out,",");
		}
	}
		
	if ( 0 == noOutputStdout ) {	
		fprintf(stdout,"%s,",thisColumn.csvTitle);
	}
	if ( 0 != out ) {	
		fprintf(out,"%s,",thisColumn.csvTitle);
	}
	outputSeparatorCount = thisColumn.integerColumn;
	
return	0;
}
char *get_date_from_json( json_object *jobj ) {
	json_object *tmp = NULL;
	int rc = json_pointer_get(jobj,"/date",&tmp);
	if ( 0 == rc ) {
		return strsave((char *) json_object_to_json_string_ext(tmp, JSON_C_TO_STRING_PRETTY));
	}
	return strsave((char *) EOF_DATE);
}
int   populate_topic( TOPICS * p ) {
	if ( 0 == p->t_packet ) {
		char *packet;
		if ( 0 != QueueGet(&p->t_q,(void **) &packet)) {
			return  1;
		}
		json_object *jobj = parse_a_string(packet);
		if ( 0 == jobj ) {
			fprintf(stderr,"# parse_a_string() %s failed.\n",packet);
			return  1;
		}
		char *date = get_date_from_json(jobj);
		if ( 0 == date ) {
			fprintf(stderr,"# get_date_from_json() %s failed.\n",packet);
			return  1;
		}
		if ( 0 == strcmp(EOF_DATE,date) ) {
			free(packet);
			packet = 0;
			json_object_put(jobj);
			jobj = 0;
		}

		p->t_packet = packet;
		p->t_date = date;
		p->new_jobj = jobj;
		return 0;
	}
	return 1;
}


void populate_topic_date(TOPICS * p ) {
	if ( 0 == p ) {
		return;
	}

	populate_topic_date(p->left);
	populate_topic(p);
	populate_topic_date(p->right);
}

static	void exchange(char *a,char *b,int n) {
	register char	t;
	char	*end = a +n;

	for ( ; a < end ; a++,b++ ) {
		t = *a;
		*a = *b;
		*b = t;
	}
}


void reheap(char *array,int n,int size,int (*cmp)(char *, char *)) {
	int i,j;

	if ( 2 > n ) {
		return;
	}
	for(i=0;2*i<n;i=j) {
		j=2*i;
		if(j<n-1 && (*cmp)(array+(j*size),array+(j+1)*size) > 0 ) {
			j++;
		}
		if ( (*cmp)(array+(i*size),array+(j*size)) <= 0 ) {
			break;
		}

		exchange(array+(j*size),array+(i*size),size);
	}
}
int calc_time_diff(int a, char *then ) {
	int b;
	if ( 0 != strchr(then,EOF_DATE[0]) ) {
		return -1;
	}
	b = timestamp_to_seconds(then);

	return a - b;
}

json_object *get_next_json_object( TOPICS *p ) {
	int rc = 0;
	static int randomizer;
	p->randomizer = ++randomizer % topics_heapCount;
	if ( 0 != p->t_packet ) {
		free(p->t_packet);
		p->t_packet = 0;
	}
	if ( 0 != p->t_date ) {
		free(p->t_date);
		p->t_date = 0;
	}
	rc = populate_topic(p);
	if ( 0 == rc) {
		return p->new_jobj;
	} else {
		p->t_date = strsave((char*) EOF_DATE);
	}
	return	0;
}
void message_callback(struct mosquitto *mosq, void *obj, const struct mosquitto_message *message) {

#if 0
	if ( findTopicColumns(message->topic, message->payload, message->payloadlen) ) {
		fprintf(stderr,"# cannot find column topic %s\n",message->topic);
		exit(0);
	}
#endif
	TOPICS *t = findTOPIC(message->topic);
	if ( 0 == t ) {
		fprintf(stderr,"# cannot find topic queue %s\n",message->topic);
		exit(0);
	}
	if ( 0 != QueuePut(&t->t_q,strsave(message->payload))) {
		fprintf(stderr,"# %s queue is full\n",message->topic);
	}
	if ( 0 == t->t_packet ) {
		get_next_json_object(t);
	}

}
int datetimecmp( TOPICS **a, TOPICS **b ) {
	int rc = strcmp(a[0]->t_date,b[0]->t_date);
	if ( 0 == rc ) {
		rc = a[0]->randomizer - a[0]->randomizer;
	}
	return	rc;	
}
char * get_timestamp(void ) {
	static char timestamp[64];
	struct tm *now;
	struct timeval time;
        gettimeofday(&time, NULL);

	now = localtime(&time.tv_sec);
	if ( 0 == now ) {
		fprintf(stderr,"# error calling localtime() %s",strerror(errno));
		exit(1);
	}

	snprintf(timestamp,sizeof(timestamp),"%04d-%02d-%02d %02d:%02d:%02d",
		1900 + now->tm_year,1 + now->tm_mon, now->tm_mday,now->tm_hour,now->tm_min,now->tm_sec);

	return timestamp;
}
int csvOutputFunc(char *last_date ) {
	int i;
	char *p = strchr(last_date,'.');
	if ( 0 != p ) {
		memcpy(p,".999",4);
	}


	FILE *out = _openLogfile(last_date);
	if ( 0 != out ) {
		fputs(last_date,out);
		fputc(',',out);
	}
	if ( 0 == noOutputStdout ) {
		fputs(last_date,stdout);
		fputc(',',stdout);
	}
	outputSeparatorCount = 1;
	for ( i = 0; columnsCount > i; i++ ) {
		if ( 0 != columns[i].debug ) {
			_columnDebugCount++;
		}
		outputThisColumn(i,out);
	}
	if ( 0 != out ) {
		fputs("\n",out);
		fclose(out);
	}
	if ( 0 == noOutputStdout ) {
		fputs("\n",stdout);
		fflush(stdout);
	}
	


return	false;
}
void output_queue_usage(void) {
	int i;
	static int count;
	if ( 0 != outputDebug ) {
		fprintf(stderr,"# --------------------- %d --------------------------- output_queue_usage()\n",count++ );
		for ( i = 0; topics_heapCount > i; i++ ) {
			fprintf(stderr,"# %d %s %s\n",
			QueueCount(&topics_heap[i]->t_q),topics_heap[i]->topic,topics_heap[i]->t_date);
		}
	}
}
#define QSRTCMPCAST  (int (*)(const void *, const void *))
void process_records_inorder(void ) {
	/* topics_heap[0] is the oldest record so process it then replace it */
	char last_date[48];
	int count = 0;
	
	qsort(topics_heap,topics_heapCount,sizeof(TOPICS *),QSRTCMPCAST datetimecmp);
	strncpy(last_date,topics_heap[0]->t_date,sizeof(last_date));
	output_queue_usage();
	if ( 0 == strcmp(last_date,EOF_DATE) ) {
		/* there are no records to process */
		return;
	}
	do {
		count++;
		if ( 0 != strncmp(last_date,topics_heap[0]->t_date,21) ) {
			if ( 0 != outputDebug ) {
				 fprintf(stderr,"############################################################\n");
			}
			/* okay this breaks at every second so this is where we output csv */
			if ( '\0' != last_date[0] ) {
				csvOutputFunc(last_date);
				if ( 0 != outputDebug ) {
					fprintf(stderr,"# %s %s\n",last_date,get_timestamp());
				}
				output_queue_usage();
				return;
			}
		}
		strncpy(last_date,topics_heap[0]->t_date,sizeof(last_date));
		 if ( 0 != topics_heap[0]->t_jobj) {
			json_object_put(topics_heap[0]->t_jobj);	/* get of old one before replacement */
		}
		topics_heap[0]->t_jobj = topics_heap[0]->new_jobj;
		topics_heap[0]->new_jobj = 0;
		if ( 0 != outputDebug ) {
			 fprintf(stderr,"# %s\n",
					 json_object_to_json_string_ext(topics_heap[0]->t_jobj, JSON_C_TO_STRING_PLAIN));
		}

		 if ( 0 != topics_heap[0]->new_jobj) {
			json_object_put(topics_heap[0]->new_jobj);	/* get of old one before replacement */
			topics_heap[0]->new_jobj = 0;
		}
		json_object *jobj; 
		if  (  0 == (jobj = get_next_json_object(topics_heap[0])) ) {
			//topics_heap[0]->t_date = strsave((char *) EOF_DATE);
		} 
		reheap((char *) topics_heap,topics_heapCount,sizeof(TOPICS *),(int (*)(char *,char *))datetimecmp);
		output_queue_usage();
	} while ( 1 || 0 != strcmp(EOF_DATE,topics_heap[0]->t_date ));

}
int do_csvOutput(void) {
	struct timeval time;
        gettimeofday(&time, NULL);
	static int count;



	/* time_diff is the number of seconds between oldest record and real time */
	int time_diff = calc_time_diff(time.tv_sec,topics_heap[0]->t_date+1);

	/* 4< time_diff means that at least 4 seconds have passed after real time */
	/* 0 > time_diff means we hit enod of stream for all topics but keep looking  */
	if ( 0 > time_diff &&  0 != ( count++ % 5 )) {
		return	false;
	}
	if ( 4 < time_diff || 0 > time_diff ) {
		process_records_inorder();
	}

	return	false;
}
static int startup_mosquitto(void) {
	char clientid[24];
	struct mosquitto *mosq;
	int rc = 0;

	fprintf(stderr,"# mqtt-modbus start-up\n");

	fprintf(stderr,"# installing signal handlers\n");
	signal(SIGINT, signal_handler);
	signal(SIGTERM, signal_handler);
	signal(SIGALRM, signal_handler);

	fprintf(stderr,"# initializing mosquitto MQTT library\n");
	mosquitto_lib_init();

	memset(clientid, 0, 24);
	snprintf(clientid, 23, "mqtt_modbus_%d", getpid());
	mosq = mosquitto_new(clientid, true, 0);

	if (mosq) {
		int loop_interval;

		if ( 0 != mqtt_user_name && 0 != mqtt_passwd ) {
			mosquitto_username_pw_set(mosq,mqtt_user_name,mqtt_passwd);
		}
		mosquitto_connect_callback_set(mosq, connect_callback);
		mosquitto_message_callback_set(mosq, message_callback);

		fprintf(stderr,"# connecting to MQTT server %s:%d\n",mqtt_host,mqtt_port);
		rc = mosquitto_connect(mosq, mqtt_host, mqtt_port, 60);

		topics_mosquitto_subscribe(topic_root,mosq);

		if ( 0 < hertz ) {
			loop_interval = (1000/hertz) >> 1;
		} else {
			loop_interval = 500;
		}



		while (run) {
			static int whileCount;
			uint64_t latency = (uint64_t) 0 - microtime();
			rc = mosquitto_loop(mosq, loop_interval, 1);
			if ( outputDebug ) {
				latency += microtime();
				fprintf(stderr,"# mosquitto_loop() latency %lld %d\n",(long long int)latency,++whileCount);
			}

			if ( MOSQ_ERR_SUCCESS == rc ) {
				uint64_t latency = (uint64_t) 0 - microtime();
				do_csvOutput();
				if ( outputDebug ) {
					latency += microtime();
					fprintf(stderr,"# do_csvOutput() latency %lld\n",(long long int)latency);
				}
			}

			if ( run && rc ) {
				fprintf(stderr,"connection error!\n");
				fprintf(stdout,"connection error!\n");
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

char *load_file(char *fname) {
	struct stat buf;

	if ( 0 != stat(fname,&buf)) {
		fprintf(stderr,"# cannot stat %s\n",fname);
		return	0;
	}
	int size = buf.st_size;
	if ( 0 >= size ) {
		fprintf(stderr,"# %s has no size\n",fname);
		return	0;
	}
	char *buffer = calloc(1,size+4);
	if ( 0 == buffer ) {
		fprintf(stderr,"# cannot allocate %s\n",fname);
		return	0;
	}
	FILE *in = fopen(fname,"r");
	if ( 0 == in ) {
		fprintf(stderr,"# cannot open %s\n",fname);
		free(buffer);
		return	0;
	}
	int rd =  fread(buffer,1,size,in);
	fclose(in);
	if ( size != rd ) {
		fprintf(stderr,"# cannot fread %s\n",fname);
		free(buffer);
		return	0;
	}
	return	buffer;
}
static int _integerColumn(COLUMN this_column ) {
	int i;
	static int big = 0x08ff;

	if ( 0 == this_column.csvColumn[0] ) {
		return	big--;
	}
	i = this_column.csvColumn[0] -'A' + 1;
	if ( isalpha(this_column.csvColumn[1] )) {
		i *= 26;
		i += this_column.csvColumn[1] - 'A' +1;
	}
	return	i;
}
int load_column( json_object *jobj, int i ) {
	COLUMN this_column = {};
	json_object *tmp;

	if ( 0 == json_object_object_get_ex(jobj,"csvColumn",&tmp)) {
                return  1;
        }
        this_column.csvColumn = strsave((char *) json_object_get_string(tmp));
	if ( 0 != outputDebug ) {
		fprintf(stderr,"# csvColumn=%s\n",this_column.csvColumn);
	}

	if ( 0 == json_object_object_get_ex(jobj,"csvOutput",&tmp)) {
                return  1;
        }
        this_column.csvOutput = strsave((char *) json_object_get_string(tmp));
	if ( 0 != outputDebug ) {
		fprintf(stderr,"# csvOutput=%s\n",this_column.csvOutput);
	}

	if ( 0 == json_object_object_get_ex(jobj,"csvTitle",&tmp)) {
                return  1;
        }
        this_column.csvTitle = strsave((char *) json_object_get_string(tmp));
	if ( 0 != outputDebug ) {
		fprintf(stderr,"# csvTitle=%s\n",this_column.csvTitle);
	}

	if ( 0 == json_object_object_get_ex(jobj,"mqttTopic",&tmp)) {
                return  1;
        }
        this_column.mqttTopic = strsave((char *) json_object_get_string(tmp));
	if ( 0 != outputDebug ) {
		fprintf(stderr,"# mqttTopic=%s\n",this_column.mqttTopic);
	}

	if ( 0 == json_object_object_get_ex(jobj,"jsonPath",&tmp)) {
                return  1;
        }
        this_column.jsonPath = strsave((char *) json_object_get_string(tmp));
	if ( 0 != outputDebug ) {
		fprintf(stderr,"# jsonPath=%s\n",this_column.jsonPath);
	}

	this_column.integerColumn = _integerColumn(this_column);
	if ( 0 != outputDebug ) {
		fprintf(stderr,"# integerColumn=%d\n",this_column.integerColumn);
	}

	this_column.c_this_topic = add_topic(	this_column.mqttTopic );

	this_column.csvOutputType = get_csvOutputType(this_column.csvOutput);
	if ( 0 == this_column.csvOutputType ) {
		return 1;
	}
	
	if ( 0 != outputDebug ) {
		fprintf(stderr,"# this_topic->topic=%s\n",this_column.c_this_topic->topic);
	}
	/* now process the optional display configuration */
	if ( 0 != json_object_object_get_ex(jobj,"debug",&tmp)) {
                this_column.debug = json_object_get_int(tmp);
        }
	if ( 0 != json_object_object_get_ex(jobj,"csvOutputFormat",&tmp)) {
                this_column.csvOutputFormat = strsave((char *) json_object_get_string(tmp));
        }
	this_column.periodic.maximum = this_column.continuous.maximum = 0.0 - INFINITY;
	this_column.periodic.minimum = this_column.continuous.minimum = INFINITY;
	this_column.uLastUpdate = microtime();

	columns[i] = this_column;
	columnsCount++;


return	0;
}
int columnsCmp(COLUMN *a, COLUMN *b ) {
	return a->integerColumn - b->integerColumn;
}

static int _build_configuration( json_object *jobj ) {
	int i;
	if ( 0 == jobj ) {
                fprintf(stderr,"# unable parse configuration containing json\n");
                return  1;
        }
        json_object *TheseColumns = NULL;
        if ( false == json_object_object_get_ex(jobj,"columns",&TheseColumns)) {
                return  1;
        }
	for ( i = 0; json_object_array_length(TheseColumns) > i; i++ ) {
		if ( 0 != outputDebug ) {
			fprintf(stderr,"# %d\n",i);
		}
		json_object *column_obj = json_object_array_get_idx(TheseColumns,i);
		if ( NULL == column_obj ) {
			fprintf(stderr,"Cannot get array entry %d\n",i);
			return	-1;
		}
		if ( load_column(column_obj,i)) {
			fprintf(stderr,"Cannot not load column entry %d\n",i);
			return	-1;
		}
	}
	qsort(columns,	columnsCount, sizeof(COLUMN),(int (*)(const void *, const void *))columnsCmp);

	return	0;
}

static int _outputHeaders(FILE *out ) {
	int i;

	if ( 0 == noOutputStdout ) {
		fputs("DATE,",stdout);
	}
	if ( 0 != out ) {
		fputs("DATE,",out);
	}
	outputSeparatorCount = 1;
	for ( i = 0; columnsCount > i; i++ ) {
		outputThisColumnHeader(i,out);
	}
	if ( 0 == noOutputStdout ) {
		fputs("\n",stdout);
		fflush(stdout);
	}
	if ( 0 != out ) {
		fputs("\n",out);
	}


return	false;
}
		
static int _load_configuration(void) {
	char *string = load_file(configuration);

	if ( 0 == string ) {
		return	-1;
	}
	
	json_object *jobj = parse_a_string(string);

	if ( NULL == jobj ) {
		fprintf(stderr,"# %s does not contain a json object\n",configuration);
		return	-1;
	}
	if ( _build_configuration(jobj)) {
		fprintf(stderr,"# %s configuration error.\n",configuration);
		return	-1;
	}
	json_object_put(jobj);




	return	0;
}

enum arguments {
	A_mqtt_host = 512,
	A_mqtt_port,
	A_mqtt_user_name,
	A_mqtt_password,
	A_configuration,
	A_hertz,
	A_millisecond_interval,
	A_log_dir,
	A_unitary_log_file,
	A_log_file_prefix,
	A_log_file_suffix,
	A_split_log_file_by_day,
	A_quiet,
	A_verbose,
	A_help,
};

int main(int argc, char **argv) {
	int n;
	int rc;
	char	cwd[256] = {};
	(void) getcwd(cwd,sizeof(cwd));

	/* command line arguments */
	while (1) {
		// int this_option_optind = optind ? optind : 1;
		int option_index = 0;
		static struct option long_options[] = {
			/* normal program */
		        {"unitary-log-file",                 1,                 0, A_unitary_log_file },
		        {"log-dir",                          1,                 0, A_log_dir },
		        {"log-file-prefix",                  1,                 0, A_log_file_prefix },
		        {"log-file-suffix",                  1,                 0, A_log_file_suffix },
		        {"mqtt-host",                        1,                 0, A_mqtt_host },
		        {"mqtt-port",                        1,                 0, A_mqtt_port },
		        {"mqtt-user-name",                   1,                 0, A_mqtt_user_name },
		        {"mqtt-passwd",                      1,                 0, A_mqtt_password },
		        {"configuration",                    1,                 0, A_configuration },
		        {"hertz",                            1,                 0, A_hertz },
		        {"millisecond-interval",             1,                 0, A_millisecond_interval },
			{"quiet",                            no_argument,       0, A_quiet, },
			{"verbose",                          no_argument,       0, A_verbose, },
		        {"help",                             no_argument,       0, A_help, },
			{},
		};

		n = getopt_long(argc, argv, "", long_options, &option_index);

		if (n == -1) {
			break;
		}
		
		switch (n) {
			case A_log_file_prefix:	
				strncpy(logFilePrefix,optarg,sizeof(logFilePrefix));
				fprintf(stderr,"# logFilePrefix=%s \n",logFilePrefix);
				splitLogFilesByDay = 0;
				break;
			case A_split_log_file_by_day:
				splitLogFilesByDay = 1;
				fprintf(stderr,"# logFilePrefix=%s \n","DISABLED");
				break;
			case A_log_file_suffix:	
				strncpy(logFileSuffix,optarg,sizeof(logFileSuffix));
				fprintf(stderr,"# logFileSuffix=%s \n",logFileSuffix);
				break;
			case A_unitary_log_file:
				unitaryLogFile=1;
				fprintf(stderr,"# unitaryLogFile\tlogging to one directory\n");
				break;
			case A_millisecond_interval:	
				milliseconds = atoi(optarg);
				fprintf(stderr,"# %d millisecond-interval\n",milliseconds);
				break;
			case A_hertz:	
				hertz = atoi(optarg);
				fprintf(stderr,"# %d hertz\n",hertz);
				break;
			case A_configuration:	
				strncpy(configuration,optarg,sizeof(configuration));
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
			case A_log_dir:	
				strncpy(logDir,optarg,sizeof(logDir));
				break;
			case A_verbose:
				outputDebug=1;
				fprintf(stderr,"# verbose (debugging) output to stderr enabled\n");
				break;
			case A_quiet:
				noOutputStdout=1;
				fprintf(stderr,"# quiet no output to stdout\n");
				break;
			case A_help:
				fprintf(stdout,"# --configuration\t\tjson columns\tREQUIRED\n");
				fprintf(stdout,"# --mqtt-host\t\t\tmqtt-host is required\tREQUIRED\n");
				fprintf(stdout,"# --mqtt-topic\t\t\tmqtt topic must be used at least once\n");
				fprintf(stdout,"# --mqtt-port\t\t\tmqtt port\t\tOPTIONAL\n");
				fprintf(stdout,"# --log-dir\t\t\tlogging directory, default=logLocal\n");
				fprintf(stdout,"# --hertz\t\t\tfrequency of logging. default = 1\n");
				fprintf(stdout,"# --millisecond-interval\tfrequency of logging excludes hertz\n");
				fprintf(stdout,"# --display-hertz\t\tfrequency of screen update. default = off\n");
				fprintf(stdout,"#\n");
				fprintf(stdout,"# --help\t\t\tThis help message then exit\n");
				fprintf(stdout,"#\n");
				exit(0);
		}
	}
	 if ( ' ' >=  configuration[0] ) {
               fprintf(stderr, "# --configuration <required>\n");
               exit(EXIT_FAILURE);
	}
	else if ( _load_configuration() ) {
               exit(EXIT_FAILURE);
	}

	 if ( ' ' >=  mqtt_host[0] ) {
               fprintf(stderr, "# --mqtt-host <required>\n");
               exit(EXIT_FAILURE);
	}
	else
	if ( 0 == topic_root ) {
		fprintf(stderr,"# There must be at least one --mqtt-topic\n");
               exit(EXIT_FAILURE);
	}
	if ( 0 != milliseconds ) {
		hertz = 0;
	} else if ( 0 == hertz ) {
		hertz = 1;
		fprintf(stderr,"# hertz = 1\n");
	}


	/* install signal handler */
	signal(SIGALRM, signal_handler); /* timeout */
	signal(SIGUSR1, signal_handler); /* user signal to do data block debug dump */
	signal(SIGPIPE, signal_handler); /* broken TCP connection */

	if ( _enable_logging_dir(logDir)) {
		return	1;
	}
	chdir(logDir);


	rc = startup_mosquitto();
	

	chdir(cwd);

	return	rc;
}
