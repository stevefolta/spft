#include "Run.h"
#include "UTF8.h"
#include <stdlib.h>
#include <string.h>


Run::Run(Style style_in)
	: style(style_in), characters(nullptr)
{
}


Run::Run(char* initial_characters, Style style_in)
	: style(style_in), characters(initial_characters)
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
	else
		characters = (char*) malloc(num_bytes + 1);
	memcpy(characters + old_length, new_chars, num_bytes);
	characters[old_length + num_bytes] = 0;
}


void Run::replace_characters(int column, const char* new_chars, int num_bytes)
{
	int old_length = 0;
	int start_index = 0;
	int replaced_bytes = 0;
	if (characters) {
		old_length = strlen(characters);
		start_index = UTF8::bytes_for_n_characters(characters, old_length, column);
		int num_chars = UTF8::num_characters(new_chars, num_bytes);
		replaced_bytes =
			UTF8::bytes_for_n_characters(
				characters + start_index, old_length - start_index, num_chars);
		}
	char* new_bytes = (char*) malloc(old_length + num_bytes + 1);

	// Build the new string.
	// Initial old characters.
	if (start_index > 0)
		memcpy(new_bytes, characters, start_index);
	// New characters.
	memcpy(new_bytes + start_index, new_chars, num_bytes);
	// Any bytes left after the replaced characters.
	int bytes_left = old_length - (start_index + replaced_bytes);
	if (bytes_left > 0) {
		memcpy(
			new_bytes + start_index + num_bytes,
			characters + start_index + replaced_bytes,
			bytes_left);
		new_bytes[start_index + num_bytes + bytes_left] = 0;
		}
	else
		new_bytes[start_index + num_bytes] = 0;

	free(characters);
	characters = new_bytes;
}


void Run::append_spaces(int num_spaces)
{
	int old_length = 0;
	if (characters) {
		old_length = strlen(characters);
		characters = (char*) realloc(characters, old_length + num_spaces + 1);
		}
	else
		characters = (char*) malloc(num_spaces + 1);
	memset(characters + old_length, ' ', num_spaces);
	characters[old_length + num_spaces] = 0;
}


void Run::shorten_to(int new_length)
{
	// Don't bother to reallocate.
	int end_index =
		UTF8::bytes_for_n_characters(characters, strlen(characters), new_length);
	characters[end_index] = 0;
}


void Run::delete_first_characters(int num_chars)
{
	int old_num_bytes = strlen(characters);
	int index =
		UTF8::bytes_for_n_characters(characters, old_num_bytes, num_chars);
	memmove(characters, characters + index, old_num_bytes - index + 1); 	// Include terminating zero byte.
	characters = (char*) realloc(characters, index + 1);
}


void Run::delete_characters(int column, int num_chars)
{
	// This is not called unless it has been determined that the deletion is
	// entirely within the run.
	int old_num_bytes = strlen(characters);
	int start_index =
		UTF8::bytes_for_n_characters(characters, old_num_bytes, column);
	int end_index =
		start_index +
		UTF8::bytes_for_n_characters(
			characters + start_index, old_num_bytes - start_index, num_chars);
	memmove(
		characters + start_index,
		characters + end_index,
		old_num_bytes - end_index);
	characters[old_num_bytes - (end_index - start_index)] = 0;
}



