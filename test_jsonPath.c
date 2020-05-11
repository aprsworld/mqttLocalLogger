

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


json_object *parse_a_string(char *string ) {

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
	return	jobj;
}
int main(int argc, char **argv ) {
	if ( 3 > argc ) {
		fprintf(stderr,"# %s <json_object> <jsonPath>\n",argv[0]);
		return	1;
	}
	json_object *jobj = parse_a_string(argv[1]);
	if ( 0 == jobj ) {
		fprintf(stderr,"# %s is not a json object\n",argv[1] );
		return	1;
	}

	fprintf(stderr,"# parse_a_string() output = %s\n",json_object_to_json_string_ext(jobj, JSON_C_TO_STRING_PRETTY));

	json_object *tmp = NULL;

	fprintf(stderr,"# jsonPath  = '%s'\n",argv[2]);

	int rc = json_pointer_get(jobj,argv[2],&tmp);

	if ( 0 == rc ) {
		fprintf(stderr,"# json_pointer_get() output = %s\n",json_object_to_json_string_ext(tmp, JSON_C_TO_STRING_PRETTY));
	}

	fprintf(stderr,"# %d\n",rc);

	return	rc;

}
