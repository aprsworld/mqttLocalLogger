
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

#define ALARM_SECONDS 600
static int run = 1;
static int mqtt_port=1883;
static char mqtt_host[256];
static char configuration[256];
static char logDir[256]="logLocal";
static char *mqtt_user_name,*mqtt_passwd;
static int unitaryLogFile;
static char logFilePrefix[256];
static char logFileSuffix[256] = ".csv";
int outputDebug=0;
int outputSeparatorCount;
int hertz = 0;
int milliseconds;
int noOutputStdout =0;
static int splitLogFilesByDay = 1;	

char last_packet_received[256];
extern char *optarg;
extern int optind, opterr, optopt;

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
	char *packet;
	int packetCount;
	json_object *jobj;
	}	TOPICS;

TOPICS *topic_root = 0;

TOPICS * add_topic(char *s ) {
	TOPICS *p,*q;
	int	cond;
	int	flag;

	if ( 0 == topic_root ) {
		topic_root = calloc(sizeof(TOPICS),1);
		topic_root->topic = strsave(s);
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

static FILE * _openLogfile(void) {
	FILE	*out;
	char	fname[256];
	char	prefix[256] = {};
	char	suffix[256] = {};
	char	path[256] = {};
	struct tm *now;
	struct timeval time;
        gettimeofday(&time, NULL);

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
		snprintf(fname,sizeof(fname),"%s/%s%s",logDir,prefix,suffix);
	}
	else snprintf(fname,sizeof(fname),"%s/%s%s",logDir,prefix,suffix);
		
	out = fopen(fname,"a");
	if ( 0 == out )	{
		fprintf(stderr,"# error calling fopen(%s) %s\n",fname,strerror(errno));
		exit(1);
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
	value = 1,

	/* 100 and above required  require building a json_object from the packet */
	count = 100,
	mean,
	sum,
	standard_deviation,
};
typedef struct {
	enum Outputs value;
	char *key;
} OutputTypes;

OutputTypes OutputTypesTable[] = {
	{value,"value",},
	{count,"count",},
	{mean,"mean",},
	{sum,"sum",},
	{standard_deviation,"standard_deviation",},
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
	char *csvColumn;
	char *csvOutput;
	char *csvTitle;
	char *mqttTopic;
	char *jsonPath;
	/* internal elements */
	int integerColumn;
	TOPICS *this_topic;
	enum Outputs csvOutputType;
	int count;
	double sum;
	/* internal standard_deviation stuff */
	double m,s;	/* m is the running mean and s is the running standard_deviation */
	int n;	/* m, s, and n must be zeroed after each output */
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
	do {
		stringlen = strlen(mystring);
		jobj = json_tokener_parse_ex(tok, mystring, stringlen);
	} while ((jerr = json_tokener_get_error(tok)) == json_tokener_continue);
	if (jerr != json_tokener_success) {
		fprintf(stderr, "Error: %s\n", json_tokener_error_desc(jerr));
		// Handle errors, as appropriate for your application.
	}
	if (tok->char_offset < stringlen) {
		// Handle extra characters after parsed object as desired.
		// e.g. issue an error, parse another object from that point, etc...
	}
	if ( outputDebug ) {
		fprintf(stderr,"# parse_a_string() output = %s\n",json_object_to_json_string_ext(jobj, JSON_C_TO_STRING_PRETTY));
		latency += microtime();
		fprintf(stderr,"# parse_a_string() latency %ld\n",latency);
	}
	return	jobj;
}
void updateColumnStats( COLUMN *this_column,  TOPICS *this_topic ) {
	if ( count > this_column->csvOutputType ) {
		return;	/* no stats for this column */
	}
	/* we now must build a json_object */
	this_column->this_topic->jobj = parse_a_string(this_column->this_topic->packet);
	int rc = -1;
	json_object *tmp = NULL;
	if ( 0 !=  this_column->this_topic->jobj) {
		rc = json_pointer_get(this_column->this_topic->jobj,this_column->jsonPath,&tmp);
	}
	if ( 0 == rc ) { /* this element exist  so we can stat this element */
		double x,previous_mean;
		switch ( this_column->csvOutputType ) {
			case	count:
				this_column->count++;
				break;
			case	mean:
				this_column->count++;
				this_column->sum += atof(json_object_to_json_string_ext(tmp, JSON_C_TO_STRING_PRETTY));
				break;
			case	sum:
				this_column->sum += atof(json_object_to_json_string_ext(tmp, JSON_C_TO_STRING_PRETTY));
				break;
			case	standard_deviation:
				x = atof(json_object_to_json_string_ext(tmp, JSON_C_TO_STRING_PRETTY));
				previous_mean = this_column->m;
				this_column->n++;
				this_column->m = ( x - this_column->m) /this_column->n;
				this_column->s += ( x - this_column->m) * ( x - previous_mean);
				break;
				
		}
	}

}
int findTopicColumns(char *s, char *packet, int packetlen ) {
	int i = 0;
	int rc = true;
	if ( outputDebug ) {
		fprintf(stderr,"# findTopicColumns '%s'\n",packet);
	}
	for ( i = 0; columnsCount > i; i++ ) {
		if ( 0 == strcmp(columns[i].mqttTopic,s) ) {
			TOPICS *this_topic = columns[i].this_topic;
			if ( 0 == this_topic ) {
				fprintf(stderr,"# internal error findTopicColumns\n");
				exit(1);
			}
			if ( 0 != this_topic->packet ) {
				free(this_topic->packet);
				this_topic->packet = 0;
			}
			char *tmp = calloc(1,packetlen + 1);
			if ( 0 != tmp ) {
				memcpy(tmp,packet,packetlen);
			}
			this_topic->packet = tmp;
			this_topic->packetCount++;;
			updateColumnStats( columns + i,this_topic);
			rc = false;
		}
	}
		
	return	rc;
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

void connect_callback(struct mosquitto *mosq, void *obj, int result) {
	if ( 5 == result ) {
		fprintf(stderr,"# --mqtt-user-name and --mqtt-passwd required at this site.\n");
	}
	if ( outputDebug ) {
		fprintf(stderr,"# connect_callback, rc=%d\n", result);
	}
}


void message_callback(struct mosquitto *mosq, void *obj, const struct mosquitto_message *message) {

#if 0
	char *debug_topic = getenv("debug_topic");

	if ( 0 != debug_topic && 0 == strcmp(message->topic,debug_topic)) {
		fprintf(stderr,"# debug_topic found in message_callback\n");
	}
#endif
	if ( findTopicColumns(message->topic, message->payload, message->payloadlen) ) {
		fprintf(stderr,"# cannot find column topic %s\n",message->topic);
		exit(0);
	}
}

static void column_clear(void) {
	int i;
	for ( i = 0; columnsCount > i ; i++ ) {
		COLUMN * t = columns + i;
		t->n = t->count = 0;
		t->s = t->m = t->sum = 0.0;
	}
}	

void output_clear(TOPICS *p) {
	if ( 0 == p ) {
		return;
	}
	output_clear(p->left);
	if ( 0 != p->packet ) {
		free(p->packet);
		p->packet = 0;
	}
	if ( 0 != p->jobj ) {
		json_object_put(p->jobj);
		p->jobj = NULL;
	}
	output_clear(p->right);
}
void clear_all_outputs(void) {
	output_clear(topic_root);
	column_clear();
}
void topics_mosquitto_subscribe(TOPICS *p, struct mosquitto *mosq)
{
if ( 0 == p )	return;
topics_mosquitto_subscribe(p->left,mosq);
mosquitto_subscribe(mosq, NULL, p->topic, 2);
topics_mosquitto_subscribe(p->right,mosq);
}
void outputThisJson( json_object *jobj, COLUMN thisColumn, FILE *out ) {
	char buffer[64] = {};
	const char *tmp = buffer;
	switch ( thisColumn.csvOutputType ) {	
		case value:
			tmp = json_object_to_json_string_ext(jobj, JSON_C_TO_STRING_PRETTY);
			break;
		case count:
			snprintf((char *)tmp,sizeof(buffer),"%d",thisColumn.count);
			break;
		case mean:
			snprintf((char *)tmp,sizeof(buffer),"%lf",thisColumn.sum/thisColumn.count);
			break;
		case sum:
			snprintf((char *)tmp,sizeof(buffer),"%lf",thisColumn.sum);
			break;
		case standard_deviation:
			snprintf((char *)tmp,sizeof(buffer),"%lf",( 0 == thisColumn.n) ? NAN :
				pow((thisColumn.s/thisColumn.n),0.5));
			break;
			
	}
	if ( 0 == noOutputStdout ) {
		fprintf(stdout,"%s,",tmp);
	}
	if ( 0 != out ) {
		fprintf(out,"%s,",tmp);
	}
	
}
int outputThisColumn( int idx, FILE *out ) {
	COLUMN thisColumn = columns[idx];

	if ( 0 == thisColumn.this_topic->jobj && 0 != thisColumn.this_topic->packet ) {
		thisColumn.this_topic->jobj = parse_a_string(thisColumn.this_topic->packet);
	}
	/* now grab the data for this column */
	json_object *tmp = NULL;
	int rc = -1;
	if ( 0 != thisColumn.this_topic->jobj ) {
		rc = json_pointer_get(thisColumn.this_topic->jobj,thisColumn.jsonPath,&tmp);
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
		
	

	outputThisJson(tmp,thisColumn,out);
	outputSeparatorCount = thisColumn.integerColumn;
	
return	0;
}
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
			//t2 = ((uint64_t)trigger_time->tv_sec * 1000000) + trigger_time->tv_usec;
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
		fprintf(stderr,"# next_msec latency %ld\n",latency);
	}
			
		
	return	false;	/* we have been triggered. */	

}
int outputThisColumnHeader( int idx, FILE *out ) {
	COLUMN thisColumn = columns[idx];


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
static int _outputHeaders(void) {
	int i;
	FILE *out = _openLogfile();
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
		fclose(out);
	}

	clear_all_outputs();

return	false;
}
int do_csvOutput(void) {
	int i;
	char timestamp[64];
	struct tm *now;
	struct timeval time;
        gettimeofday(&time, NULL);

	if ( 0 < hertz ) {
		static struct timeval hertz_tv;
		if ( next_hertz(&time,&hertz_tv) ) {
			return	0;
		}	
	}
	if ( 0 < milliseconds ) {
		static struct timeval msec_tv;
		if ( next_msec(&time,&msec_tv) ) {
			return	0;
		}	
	}


	/* cancel pending alarm */
	alarm(0);
	now = localtime(&time.tv_sec);
	if ( 0 == now ) {
		fprintf(stderr,"# error calling localtime() %s",strerror(errno));
		exit(1);
	}

	snprintf(timestamp,sizeof(timestamp),"%04d-%02d-%02d %02d:%02d:%02d,",
		1900 + now->tm_year,1 + now->tm_mon, now->tm_mday,now->tm_hour,now->tm_min,now->tm_sec);
	FILE *out = _openLogfile();
	if ( 0 == noOutputStdout ) {
		fputs(timestamp,stdout);
	}
	if ( 0 != out ) {
		fputs(timestamp,out);
	}
	outputSeparatorCount = 1;
	for ( i = 0; columnsCount > i; i++ ) {
		outputThisColumn(i,out);
	}
	if ( 0 == noOutputStdout ) {
		fputs("\n",stdout);
		fflush(stdout);
	}
	if ( 0 != out ) {
		fputs("\n",out);
		fclose(out);
	}
	

	// fprintf(stdout,"# lastPacket_received.  '%s'\n",last_packet_received);
	clear_all_outputs();

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
		if ( 0 != mosq,mqtt_user_name && 0 != mqtt_passwd ) {
			mosquitto_username_pw_set(mosq,mqtt_user_name,mqtt_passwd);
		}
		mosquitto_connect_callback_set(mosq, connect_callback);
		mosquitto_message_callback_set(mosq, message_callback);

		fprintf(stderr,"# connecting to MQTT server %s:%d\n",mqtt_host,mqtt_port);
		rc = mosquitto_connect(mosq, mqtt_host, mqtt_port, 60);

		topics_mosquitto_subscribe(topic_root,mosq);

		while (run) {
			static int whileCount;
			uint64_t latency = (uint64_t) 0 - microtime();
			rc = mosquitto_loop(mosq, -1, 1);
			if ( outputDebug ) {
				latency += microtime();
				fprintf(stderr,"# mosquitto_loop() latency %ld %d\n",latency,++whileCount);
			}

			if ( MOSQ_ERR_SUCCESS == rc ) {
				uint64_t latency = (uint64_t) 0 - microtime();
				do_csvOutput();
				if ( outputDebug ) {
					latency += microtime();
					fprintf(stderr,"# do_csvOutput() latency %ld\n",latency);
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

	this_column.this_topic = add_topic(	this_column.mqttTopic );

	this_column.csvOutputType = get_csvOutputType(this_column.csvOutput);
	if ( 0 == this_column.csvOutputType ) {
		return 1;
	}
	
	if ( 0 != outputDebug ) {
		fprintf(stderr,"# this_topic->topic=%s\n",this_column.this_topic->topic);
	}
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
	A_mqtt_topic,
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
		        {"mqtt-topic",                       1,                 0, A_mqtt_topic },
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
			case A_mqtt_topic:
				add_topic(optarg);
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
				fprintf(stdout,"# --mqtt-host\t\t\tmqtt-host is required\tREQUIRED\n");
				fprintf(stdout,"# --mqtt-topic\t\t\tmqtt topic must be used at least once\n");
				fprintf(stdout,"# --mqtt-port\t\t\tmqtt port\t\tOPTIONAL\n");
				fprintf(stdout,"# --log-dir\t\t\tlogging directory, default=logLocal\n");
				fprintf(stdout,"# --hertz\t\t\tfrequency of reporting default = 1\n");
				fprintf(stdout,"# --millisconed-interval\t\t\tfrequency of reporting excludes hertz\n");
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
	}


	/* install signal handler */
	signal(SIGALRM, signal_handler); /* timeout */
	signal(SIGUSR1, signal_handler); /* user signal to do data block debug dump */
	signal(SIGPIPE, signal_handler); /* broken TCP connection */

	if ( _enable_logging_dir(logDir)) {
		return	1;
	}
	_outputHeaders();
	chdir(logDir);


	chdir(cwd);
	rc = startup_mosquitto();
	


	return	rc;
}
