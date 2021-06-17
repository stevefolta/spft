#include "Line.h"
#include "Run.h"


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




