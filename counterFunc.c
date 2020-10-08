#include "counterFunc.h"
static int _counter0;


void resetCounter0(void) {
	_counter0 = 0;
}
void incrementCounter0(void) {
	_counter0++;
}
int getCounter0(void) {
	return _counter0;
}
