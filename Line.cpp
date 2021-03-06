#include "Line.h"
#include "Run.h"
#include "UTF8.h"
#include <sstream>
#include <stdlib.h>
#include <string.h>
#include <assert.h>


Line::Line()
	: elastic_tabs(nullptr)
{
}


Line::~Line()
{
	for (auto& run: runs)
		delete run;
	if (elastic_tabs)
		elastic_tabs->release();
}


void Line::append_characters(const char* bytes, int length, Style style)
{
	Run* last_run = nullptr;
	if (runs.empty() || (runs.back()->style != style || runs.back()->is_tab)) {
		last_run = new Run(style);
		runs.push_back(last_run);
		}
	else
		last_run = runs.back();
	last_run->append_characters(bytes, length);
}


void Line::replace_characters(int column, const char* bytes, int length, Style style)
{
	if (runs.empty()) {
		Run* run = new Run(style);
		run->append_characters(bytes, length);
		runs.push_back(run);
		return;
		}

	// First insert the new characters.
	int num_chars = UTF8::num_characters(bytes, length);
	int columns_left = column;
	int already_deleted_chars = 0;
	for (auto run = runs.begin(); run != runs.end(); ++run) {
		int old_run_chars = (*run)->num_characters();
		if (columns_left < old_run_chars) {
			if ((*run)->style == style && !(*run)->is_tab) {
				(*run)->replace_characters(columns_left, bytes, length);
				if (columns_left + num_chars <= old_run_chars) {
					// The replacement was entirely within one run; we're done.
					return;
					}
				already_deleted_chars = old_run_chars - columns_left;
				}
			else {
				auto insert_before = runs.end();
				if (columns_left == 0) {
					// New characters go rignt before this run.
					insert_before = run;
					}
				else if (columns_left + num_chars < old_run_chars) {
					// The replacement is entirely within one run; we'll need to split it.
					// Create the new run from the second part of the existing run.
					const char* run_bytes = (*run)->bytes();
					int run_bytes_length = strlen(run_bytes);
					const char* src_bytes_start =
						run_bytes +
						UTF8::bytes_for_n_characters(
							run_bytes, run_bytes_length, columns_left + num_chars);
					int src_length = run_bytes + run_bytes_length - src_bytes_start;
					Run* new_run = new Run((*run)->style);
					new_run->append_characters(src_bytes_start, src_length);
					// Add it.
					auto where = run;
					where++;
					runs.insert(where, new_run);
					// Trim the existing run.
					(*run)->shorten_to(columns_left);
					already_deleted_chars = num_chars;
					// New characters go before the next run (the one we just split
					// off).
					insert_before = run;
					insert_before++;
					}
				else {
					// Replacing the end of this run (and maybe some more after
					// that).
					(*run)->shorten_to(columns_left);
					already_deleted_chars = old_run_chars - columns_left;
					// New characters go before the next run.
					insert_before = run;
					insert_before++;
					}
				// Create new run.
				Run* new_run = new Run(style);
				new_run->append_characters(bytes, length);
				// Add it.
				runs.insert(insert_before, new_run);
				}
			break;
			}
		columns_left -= old_run_chars;
		}

	// Then delete any leftover old ones.
	if (already_deleted_chars < num_chars)
		delete_characters(column + num_chars, num_chars - already_deleted_chars);
}


void Line::insert_characters(int column, const char* bytes, int length, Style style)
{
	if (runs.empty()) {
		Run* run = new Run(style);
		run->append_characters(bytes, length);
		runs.push_back(run);
		return;
		}

	int columns_left = column;
	for (auto run = runs.begin(); run != runs.end(); ++run) {
		int old_run_chars = (*run)->num_characters();
		if (columns_left == old_run_chars) {
			if ((*run)->style == style && !(*run)->is_tab) {
				// Add characters to the end of the run.
				(*run)->append_characters(bytes, length);
				break;
				}
			}
		else if (columns_left < old_run_chars) {
			if ((*run)->style == style && !(*run)->is_tab)
				(*run)->insert_characters(columns_left, bytes, length);
			else {
				if (columns_left > 0) {
					// We'll need to split the existing run.
					split_run_at(run, columns_left);
					// Insert between the old part and the new part.
					run++;
					}
				// Insert the new run at this point.
				Run* new_run = new Run(style);
				new_run->append_characters(bytes, length);
				runs.insert(run, new_run);
				}
			break;
			}

		columns_left -= old_run_chars;
		}
}


void Line::append_tab(Style style)
{
	Run* run = new Run(strdup("\t"), style);
	run->is_tab = true;
	runs.push_back(run);
}


void Line::replace_character_with_tab(int column, Style style)
{
	Run* tab_run = new Run(strdup("\t"), style);
	tab_run->is_tab = true;

	if (runs.empty()) {
		runs.push_back(tab_run);
		return;
		}

	int columns_left = column;
	for (auto run = runs.begin(); run != runs.end(); ++run) {
		int old_run_chars = (*run)->num_characters();
		if (columns_left < old_run_chars) {
			auto insert_before = runs.end();
			if (columns_left == 0) {
				// New characters go rignt before this run.
				insert_before = run;
				}
			else if (columns_left + 1 < old_run_chars) {
				// Splitting the run.
				// Create the new run from the second part of the existing run.
				const char* run_bytes = (*run)->bytes();
				int run_bytes_length = strlen(run_bytes);
				const char* src_bytes_start =
					run_bytes +
					UTF8::bytes_for_n_characters(
						run_bytes, run_bytes_length, columns_left + 1);
				int src_length = run_bytes + run_bytes_length - src_bytes_start;
				Run* new_run = new Run((*run)->style);
				new_run->append_characters(src_bytes_start, src_length);
				// Add it.
				auto where = run;
				where++;
				runs.insert(where, new_run);
				// Trim the existing run.
				(*run)->shorten_to(columns_left);
				// The tab goes before the next run (the one we just split off).
				insert_before = run;
				insert_before++;
				}
			else {
				// Replacing the last character of this run.
				(*run)->shorten_to(columns_left);
				// Tab goes before the next run.
				insert_before = run;
				insert_before++;
				}
			// Add it.
			runs.insert(insert_before, tab_run);
			break;
			}
		columns_left -= old_run_chars;
		}
}


void Line::clear()
{
	for (auto& run: runs)
		delete run;
	runs.clear();
}


int Line::num_characters()
{
	int num_chars = 0;
	for (auto& run: runs)
		num_chars += run->num_characters();
	return num_chars;
}


void Line::get_character(int column, char* char_out)
{
	for (auto& run: runs) {
		const char* p = run->bytes();
		if (p == nullptr)
			continue;
		const char* end = p + strlen(p);
		while (p < end) {
			int char_length = UTF8::bytes_for_n_characters(p, end - p, 1);
			if (column == 0) {
				memcpy(char_out, p, char_length);
				char_out[char_length] = 0;
				return;
				}
			column -= 1;
			p += char_length;
			}
		}
	*char_out = 0;
}


std::string Line::characters_from_to(int start_column, int end_column)
{
	std::stringstream result;
	for (auto& run: runs) {
		if (end_column <= 0)
			break;
		if (start_column < 0)
			start_column = 0;
		int run_num_chars = run->num_characters();
		if (start_column < run_num_chars) {
			const char* run_bytes = run->bytes();
			int run_num_bytes = strlen(run_bytes);
			const char* start =
				run_bytes +
				UTF8::bytes_for_n_characters(
					run_bytes, run_num_bytes, start_column);
			const char* end =
				run_bytes +
				UTF8::bytes_for_n_characters(
					run_bytes, run_num_bytes, end_column);
			result << std::string(start, end);
			}
		start_column -= run_num_chars;
		end_column -= run_num_chars;
		}
	return result.str();
}


void Line::clear_to_end_from(int column)
{
	// Shorten the run containing the column, and delete all runs after it.
	auto first_to_delete = runs.end();
	bool deleting = false;
	for (auto run = runs.begin(); run != runs.end(); ++run) {
		if (deleting)
			delete *run;
		else {
			int run_num_chars = (*run)->num_characters();
			if (column == 0) {
				first_to_delete = run;
				deleting = true;
				delete *run;
				}
			else if (run_num_chars > column) {
				(*run)->shorten_to(column);
				deleting = true;
				first_to_delete = run;
				first_to_delete++;
				}
			column -= run_num_chars;
			}
		}
	runs.erase(first_to_delete, runs.end());
}


void Line::clear_from_beginning_to(int column)
{
	// Delete all runs up to the one containing the column, and remove the
	// initial characters from that one.
	while (column >= 0 && !runs.empty()) {
		auto run = runs.front();
		int run_num_chars = run->num_characters();
		if (column >= run_num_chars) {
			delete run;
			runs.pop_front();
			}
		else {
			run->delete_first_characters(column);
			break;
			}
		column -= run_num_chars;
		}
}


void Line::prepend_spaces(int num_spaces, Style style)
{
	if (num_spaces <= 0)
		return;

	char* spaces = (char*) malloc(num_spaces + 1);
	for (int i = 0; i < num_spaces; ++i)
		spaces[i] = ' ';
	spaces[num_spaces] = 0;
	runs.push_front(new Run(spaces, style));
}


void Line::append_spaces(int num_spaces, Style style)
{
	if (!runs.empty() && runs.back()->style == style)
		runs.back()->append_spaces(num_spaces);
	else {
		char* spaces = (char*) malloc(num_spaces + 1);
		for (int i = 0; i < num_spaces; ++i)
			spaces[i] = ' ';
		spaces[num_spaces] = 0;
		runs.push_back(new Run(spaces, style));
		}
}


void Line::delete_characters(int column, int num_chars)
{
	auto first_to_delete = runs.end();
	auto deleted_runs_end = runs.end();
	for (auto run = runs.begin(); num_chars > 0 && run != runs.end(); ++run) {
		int run_num_chars = (*run)->num_characters();
		if (column < run_num_chars) {
			if (column == 0) {
				// Deleting from the start of the run.
				if (num_chars >= run_num_chars) {
					// Delete the entire run.
					if (first_to_delete == runs.end())
						first_to_delete = run;
					deleted_runs_end = run;
					deleted_runs_end++;
					delete *run;
					num_chars -= run_num_chars;
					}
				else {
					// Delete the first few characters, then we're done.
					(*run)->delete_first_characters(num_chars);
					break;
					}
				}
			else if (column + num_chars >= run_num_chars) {
				// Deletion starts at the end of this run.
				(*run)->shorten_to(column);
				num_chars -= run_num_chars - column;
				column = 0;
				}
			else {
				// Deletion is entirely within this run.
				(*run)->delete_characters(column, num_chars);
				break;
				}
			}
		else
			column -= run_num_chars;
		}
	runs.erase(first_to_delete, deleted_runs_end);
}


void Line::split_run_at(std::list<Run*>::iterator run_to_split, int column)
{
	const char* run_bytes = (*run_to_split)->bytes();
	int run_bytes_length = strlen(run_bytes);
	const char* src_bytes_start =
		run_bytes +
		UTF8::bytes_for_n_characters(
			run_bytes, run_bytes_length, column);
	int src_length = run_bytes + run_bytes_length - src_bytes_start;

	// Create the new run.
	Run* new_run = new Run((*run_to_split)->style);
	new_run->append_characters(src_bytes_start, src_length);
	// Add it.
	auto where = run_to_split;
	where++;
	runs.insert(where, new_run);
	// Trim the existing run.
	(*run_to_split)->shorten_to(column);
}




