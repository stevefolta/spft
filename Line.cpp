#include "Line.h"
#include "Run.h"
#include <stdlib.h>


Line::Line()
{
}


Line::~Line()
{
	for (auto& run: runs)
		delete run;
}


void Line::append_characters(const char* bytes, int length, Style style)
{
	Run* last_run = nullptr;
	if (runs.empty() || runs.back()->style != style) {
		last_run = new Run(style);
		runs.push_back(last_run);
		}
	else
		last_run = runs.back();
	last_run->append_characters(bytes, length);
}


void Line::clear()
{
	for (auto& run: runs)
		delete run;
	runs.clear();
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
	char* spaces = (char*) malloc(num_spaces + 1);
	for (int i = 0; i < num_spaces; ++i)
		spaces[i] = ' ';
	spaces[num_spaces] = 0;
	runs.push_front(new Run(spaces, style));
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
				}
			else {
				// Deletion is entirely within this run.
				(*run)->delete_characters(column, num_chars);
				break;
				}
			}
		column -= run_num_chars;
		}
	runs.erase(first_to_delete, runs.end());
}




