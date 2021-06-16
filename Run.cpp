#include "Run.h"
#include "UTF8.h"
#include <stdlib.h>
#include <string.h>


Run::Run(Style style_in)
	: style(style_in), characters(nullptr)
{
}


Run::~Run()
{
	free((void*) characters);
}


int Run::num_characters()
{
	if (characters == nullptr)
		return 0;
	return UTF8::num_characters(characters, strlen(characters));
}


void Run::append_characters(const char* new_chars, int num_bytes)
{
	int old_length = 0;
	if (characters) {
		old_length = strlen(characters);
		characters = (char*) realloc(characters, old_length + num_bytes + 1);
		}
	else {
		characters = (char*) malloc(num_bytes + 1);
		}
	memcpy(characters + old_length, new_chars, num_bytes);
	characters[old_length + num_bytes] = 0;
}



