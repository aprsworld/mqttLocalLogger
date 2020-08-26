

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
#include <time.h>
#include <math.h>
#include <ncurses.h>

char *program_name;
char *input_file_name;
char *output_file_name;
static char configuration[256];
char *logDir;
int outputDebug=0;
int outputSeparatorCount;
static int _columnDebugCount;
int progress_indicator = 0;

const char EOF_DATE[] = "~~~~~~~~~~~~";	/* strcmp to 3030-12-25 EOF_DATE will always be bigger */

char	*strsave(char *s ) {
	char	*ret_val = 0;

	ret_val = malloc(strlen(s)+1);
	if ( 0 != ret_val) {
		strcpy(ret_val,s);
	}
	return ret_val;	
}
enum Outputs {
	/* 1-99 are operations that do not require building a json_object from the packet */
	value = 1,	/* this is only formatable as a string */
	value_integer,
	value_double,

	/* 100 and above required  require building a json_object from the packet */
	count = 100,
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
typedef struct topics {
	struct topics *left,*right;
	FILE *in;
	char	*t_date;
	char	*topic;
	char *t_packet;
	int packetCount;
	json_object *t_jobj;  /* packet for last second */
	json_object *new_jobj;	/* packet for current second */
	}	TOPICS;

TOPICS *topic_root = 0;

#define MAX_TOPICS 128
TOPICS *topics_heap[MAX_TOPICS];
int topics_heapCount;

typedef struct {
	char *csvColumn;
	char *csvOutput;
	char *csvTitle;
	char *mqttTopic;
	char *jsonPath;
	char *csvOutputFormat;	/* optional used by snprintf */
	int integerColumn;
	TOPICS * c_this_topic;
	enum Outputs csvOutputType;
	int debug;
} COLUMN;

COLUMN columns[256];
int columnsCount;
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

json_object *parse_a_string(char *string ) {

	// fprintf(stderr,"# %s\n",string);
	json_object *jobj = NULL;
	json_tokener *tok = json_tokener_new_ex(64);
	const char *mystring = string;
	int stringlen = 0;
	enum json_tokener_error jerr;

	stringlen = strlen(mystring);
	jobj = json_tokener_parse_ex(tok, mystring, stringlen);
	jerr = json_tokener_get_error(tok);
	if ( json_tokener_continue == jerr ) {
		json_object_put(jobj);
	} else if (jerr != json_tokener_success) {
		fprintf(stderr, "Error: %s\n", json_tokener_error_desc(jerr));
		exit(1);
	}
	if (tok->char_offset < stringlen) {
		// Handle extra characters after parsed object as desired.
		// e.g. issue an error, parse another object from that point, etc...
	}
	if ( outputDebug ) {
		fprintf(stderr,"# parse_a_string() output = %s\n",json_object_to_json_string_ext(jobj, JSON_C_TO_STRING_PRETTY));
	}
	json_tokener_free(tok);

	return	jobj;
}
json_object *get_next_json_object( FILE *in ) {
	char buffer[4096] = {};
	json_object *jobj = NULL;
	int len = 0;
	char *p;

	for ( ; 0 == jobj; ) {
		if ( 0 == fgets(buffer+len,sizeof(buffer) - len, in) ) {
			break;
		}
		len = strlen(buffer);
		p = strchr(buffer,'{');
		if ( 0 != p ) {
			jobj = parse_a_string(p);
		}
	}
	if ( 0 != jobj) {
		//fprintf(stderr,"# %s\n",json_object_to_json_string_ext(jobj, JSON_C_TO_STRING_PRETTY));
		// fprintf(stderr,"# %s\n",json_object_to_json_string_ext(jobj, JSON_C_TO_STRING_PLAIN));
	}

	return	jobj;



}

#if 0
static void setup_topics(void ) {

	int i;
	COLUMN *this_column;

	for ( i = 0 ; columnsCount > i; i++ ) {
		this_column = columns + i;
		this_column->c_this_topic = add_topic(this_column->mqttTopic);
	}
}
#endif
static void init_t( TOPICS *p ) {
	char cmd[512] = {};
	int rc;
	struct stat buf;
	snprintf(cmd,sizeof(cmd),"%s/%s",p->topic,input_file_name);
	if ( 0 != stat(cmd,&buf)) {
		fprintf(stderr,"# %s %s\n",cmd,strerror(errno));
		return;
	}


	snprintf(cmd,sizeof(cmd),"/bin/gunzip --stdout %s/%s",p->topic,input_file_name);
	p->in = popen( cmd,"r");
	if ( 0 == p->in) {
		fprintf(stderr,"# %s %s\n",	strerror(errno),cmd);
		return;
	} 
	if  (   0 == (p->new_jobj = get_next_json_object(p->in)) ) {
		fprintf(stderr,"# cannot get first json object %s/%s\n",p->topic,input_file_name);
		return;
	} 
	json_object *tmp = NULL;
	rc = json_pointer_get(p->new_jobj,"/date",&tmp);
	if ( 0 == rc ) {
		p->t_date = strsave((char *) json_object_to_json_string_ext(tmp, JSON_C_TO_STRING_PRETTY));
		if ( MAX_TOPICS > topics_heapCount ) {
			topics_heap[topics_heapCount] = p;
		}
		topics_heapCount++;
	}
}
static void initialize_topics( TOPICS * p ) {
	if ( 0 == p ) {
		return;
	}
	initialize_topics(p->left);

	init_t(p);

	initialize_topics(p->right);
}
static void close_topics( TOPICS * p ) {
	if ( 0 == p ) {
		return;
	}
	close_topics(p->left);
	if ( 0 != p->in ) {
		pclose(p->in);
	}
	close_topics(p->right);
}
int datetimecmp( TOPICS **a, TOPICS **b ) {
	return strcmp(a[0]->t_date,b[0]->t_date);
}
int outputThisColumnHeader( int idx, FILE *out ) {
	COLUMN thisColumn = columns[idx];

	if ( 0 == thisColumn.csvColumn[0] ) {
		return	0;
	}


	int i = outputSeparatorCount + 1;
	for ( ; thisColumn.integerColumn > i; i++ ) {
		if ( 0 != out ) {
			fprintf(out,",");
		}
	}
		
	if ( 0 != out ) {	
		fprintf(out,"%s,",thisColumn.csvTitle);
	}
	outputSeparatorCount = thisColumn.integerColumn;
	
return	0;
}
static int _outputHeaders(FILE *out ) {
	int i;

	if ( 0 != out ) {
		fputs("DATE,",out);
	}
	outputSeparatorCount = 1;
	for ( i = 0; columnsCount > i; i++ ) {
		outputThisColumnHeader(i,out);
	}
	if ( 0 != out ) {
		fputs("\n",out);
		fflush(out);
	}


return	false;
}
static FILE * _openLogfile(void) {
	FILE	*out;
	struct stat buf;
	int flag =  stat(output_file_name,&buf);
		
	out = fopen(output_file_name,"a");
	if ( 0 == out )	{
		fprintf(stderr,"# error calling fopen(%s) %s\n",output_file_name,strerror(errno));
		exit(1);
	}
	if ( 0 != flag ) {
		_outputHeaders(fopen(output_file_name,"a"));
	}
	return	out;
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
	}
	unset_sigsegv();
	
	if ( 0 != out ) {
		fprintf(out,"%s,",buffer);
	}
	
}
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
int do_csvOutput(char *last_date ) {
	int i;
	char *p = strchr(last_date,'.');
	if ( 0 != p ) {
		memcpy(p,".999",4);
	}


#if 1
	FILE *out = _openLogfile();
	if ( 0 != out ) {
		fputs(last_date,out);
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
	
#endif


return	false;
}

void process_records_inorder(void ) {
	/* topics_heap[0] is the oldest record so process it then replace it */
	char last_date[48] = {};
	int count = 0;
	
	do {
		count++;
		if ( 0 == (count % 1000) && 0 != progress_indicator) {
			fprintf(stderr,"# %6d. %s\n",count,last_date);
		}

		if ( 0 != strncmp(last_date,topics_heap[0]->t_date,21) ) {
			if ( 0 != outputDebug ) {
				 fprintf(stderr,"############################################################\n");
			}
			/* okay this breaks at every second so this is where we output csv */
			if ( '\0' != last_date[0] ) {
				do_csvOutput(last_date);
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
		}
		json_object *jobj; 
		if ( 0 != topics_heap[0]->t_date ) {
			free(topics_heap[0]->t_date);
		}
		if  (  0 == (jobj = get_next_json_object(topics_heap[0]->in)) ) {
			topics_heap[0]->t_date = strsave((char *) EOF_DATE);
		} else {
			int rc;
			topics_heap[0]->new_jobj = jobj;
			json_object *tmp = NULL;
			rc = json_pointer_get(topics_heap[0]->new_jobj,"/date",&tmp);
			if ( 0 == rc ) {
				topics_heap[0]->t_date = strsave((char *) json_object_to_json_string_ext(tmp, JSON_C_TO_STRING_PRETTY));
			} else {
				topics_heap[0]->t_date = strsave((char *) EOF_DATE);
			}

		}
		reheap((char *) topics_heap,topics_heapCount,sizeof(TOPICS *),(int (*)(char *,char *))datetimecmp);
	} while ( 0 != strcmp(EOF_DATE,topics_heap[0]->t_date ));
	fprintf(stderr,"# end of records ###############################################\n");

}

#define QSRTCMPCAST  (int (*)(const void *, const void *))
static void process(void) {
	//  setup_topics();
	initialize_topics(topic_root);
	if ( MAX_TOPICS < topics_heapCount ) {
		fprintf(stderr,"# MAX_TOPICS %d < topics_heapCount %d\n",MAX_TOPICS,topics_heapCount);
		exit(1);
	}
	qsort(topics_heap,topics_heapCount,sizeof(TOPICS *),QSRTCMPCAST datetimecmp);
	process_records_inorder();
	close_topics(topic_root);

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
enum Outputs get_csvOutputType(char *s ) {
	OutputTypes *p = OutputTypesTable;

	for ( ; 0 != p->key && 0 != strcmp(s,p->key); p++ ) {
		}
	
	if ( 0 == p->key ) {
		fprintf(stderr,"# cannot find csvOutputType %s\n",s);
	}
	return	p->value;
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
	if ( 0 != json_object_object_get_ex(jobj,"debug",&tmp)) {
                this_column.debug = json_object_get_int(tmp);
        }
	
	if ( 0 != outputDebug ) {
		fprintf(stderr,"# this_topic->topic=%s\n",this_column.c_this_topic->topic);
	}
	if ( 0 != json_object_object_get_ex(jobj,"csvOutputFormat",&tmp)) {
                this_column.csvOutputFormat = strsave((char *) json_object_get_string(tmp));
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
int check_output_file_name(void ) {
	int rc;
	struct stat buf;

	rc = ( 0 == stat(output_file_name,&buf));
	if ( 0 != rc ) {
		fprintf(stderr,"# %s %s\n",output_file_name,"exists (no no append allowed.)");
	}

	return	rc;

}
enum arguments {
	A_configuration = 512,
	A_log_dir,
	A_input_file_name,
	A_output_file_name,
	A_progress_indicator,
	A_quiet,
	A_verbose,
	A_help,
};
int main(int argc, char **argv) {
	int n;
	int rc = 0;
	char	cwd[256] = {};
	(void) getcwd(cwd,sizeof(cwd));

	program_name = strsave(argv[0]);

	/* command line arguments */
	while (1) {
		// int this_option_optind = optind ? optind : 1;
		int option_index = 0;
		static struct option long_options[] = {
			/* normal program */
		        {"log-dir",                          1,                 0, A_log_dir },
		        {"input-file-name",                  1,                 0, A_input_file_name },
		        {"output-file-name",                 1,                 0, A_output_file_name },
		        {"configuration",                    1,                 0, A_configuration },
			{"quiet",                            no_argument,       0, A_quiet, },
			{"verbose",                          no_argument,       0, A_verbose, },
			{"progress-indicator",               no_argument,       0, A_progress_indicator, },
		        {"help",                             no_argument,       0, A_help, },
			{},
		};

		n = getopt_long(argc, argv, "", long_options, &option_index);

		if (n == -1) {
			break;
		}
		
		switch (n) {
			case A_configuration:	
				strncpy(configuration,optarg,sizeof(configuration));
				break;
			case A_input_file_name:	
				input_file_name = strsave(optarg);
				break;
			case A_output_file_name:	
				output_file_name = strsave(optarg);
				fprintf(stderr,"# output-file-name %s\n",output_file_name);
				break;
			case A_log_dir:	
				logDir  = strsave(optarg);
				break;
			case A_verbose:
				outputDebug=1;
				fprintf(stderr,"# verbose (debugging) output to stderr enabled\n");
				break;
			case A_progress_indicator:
				progress_indicator = 1;
				fprintf(stderr,"# progress indicator enabled\n");
				break;
			case A_help:
				fprintf(stdout,"# --configuration\t\tjson columns\t\t\tREQUIRED\n");
				fprintf(stdout,"# --input-file-name\t\tbasename (NO PATH)\t\tREQUIRED\n");
				fprintf(stdout,"# --output-file-name\t\tlogg-dir/name\t\t\tREQUIRED\n");
				fprintf(stdout,"# --log-dir\t\t\tlogging directory, default=logLocal\tOPTIONAL\n");
				fprintf(stdout,"# --verbose\t\t\tNO ARGS\t\t\t\tOPTIONAL\n");
				fprintf(stdout,"# --progress-indicator\t\tNO AGRS\t\t\t\tOPTIONAL\n");
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
	if ( 0 == logDir) {
		fprintf(stderr,"# --log-dir <required>\n");
               exit(EXIT_FAILURE);
	}
	if ( 0 == input_file_name) {
		fprintf(stderr,"# --input_file_name <required>\n");
               exit(EXIT_FAILURE);
	}
	if ( 0 == output_file_name) {
		fprintf(stderr,"# --output_file_name <required>\n");
               exit(EXIT_FAILURE);
	}



	chdir(logDir);

	if ( 0 != check_output_file_name() ) {
               exit(EXIT_FAILURE);
	}

	process();

	chdir(cwd);

	return	rc;
}
