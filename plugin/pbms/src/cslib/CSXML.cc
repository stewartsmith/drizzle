/* Copyright (C) 2010 PrimeBase Technologies GmbH, Germany
 *
 * PrimeBase Media Stream for MySQL
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
 *
 * Paul McCullagh (H&G2JCtL)
 *
 * 2010-01-12
 *
 * CORE SYSTEM:
 * XML Parsing
 *
 */

#include "CSConfig.h"
#include <inttypes.h>


#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdio.h>
#include <errno.h>

#ifdef DRIZZLED
#include <boost/algorithm/string.hpp>
#define STRCASESTR(s1, s2) boost::ifind_first(s1, s2)
#else
#define STRCASESTR(s1, s2) strcasestr(s1, s2)
#endif

#include "CSXML.h"

#define ISSPACE(ch)			(ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r')
#define ISSINGLE(ch)		(ch == '*' || ch == '+' || ch == '(' || ch == ')' || ch == ',' || ch == '|' || ch == '[' || ch == ']' || ch == '?' || ch == '/')

#define SET_CHAR(x, ch)		{ x->buffer[0] = ch; x->count = 1; }
#define ADD_CHAR(x, ch)		{ if (x->count < PARSE_BUFFER_SIZE) { x->buffer[x->count] = ch; x->count++; } else x->buffer[PARSE_BUFFER_SIZE-1] = ch; }

bool CSXMLParser::match_string(const char *ch)
{
	int32_t i;
	
	for (i=0; i<this->count; i++) {
		if (this->buffer[i] != *ch)
			return false;
		ch++;
	}
	if (*ch)
		return false;
	return(i == this->count);
}

void CSXMLParser::increment_nesting(wchar_t ch)
{
	if (this->nesting < PARSE_STACK_SIZE) {
		switch (ch) {
			case '/':
				this->end_type[this->nesting] = XML_OP_1_END_CLOSE_TAG;
				break;
			case '?':
				this->end_type[this->nesting] = XML_OP_1_END_PI_TAG;
				break;
			case '!':
				this->end_type[this->nesting] = XML_OP_1_END_ENTITY_TAG;
				break;
			case '[':
				this->end_type[this->nesting] = XML_OP_1_END_BRACKET_TAG;
				break;
			default:
				if (ISSPACE(ch))
					this->end_type[this->nesting] = XML_OP_1_END_UNKNOWN_TAG;
				else
					this->end_type[this->nesting] = XML_OP_1_END_TAG;
				break;
		}
	}
	this->nesting++;
}

int32_t CSXMLParser::parseChar(wchar_t ch)
/* This function does the actual work of parsing. It is expects 
 * "complete" characters as input. This could be 4 byte characters
 * as long as it is able to recognize the characters that are
 * relevant to parsing.
 * The function outputs processing instructions, and indicates
 * how the output data is to be understood.
 */
{
	switch (this->state) {
		case XML_BEFORE_CDATA:
			this->nesting = 0;
			/* This is the initial state! */
			if (ch == '<') {
				this->state = XML_LT;
				this->type = XML_noop;
			}
			else {
				this->state = XML_IN_CDATA;
				this->type = XML_CDATA_CH;
			}
			SET_CHAR(this, ch);
			break;
		case XML_IN_CDATA:
			if (ch == '<') {
				this->state = XML_LT;
				this->type = XML_noop;
			}
			else
				this->type = XML_CDATA_CH;
			SET_CHAR(this, ch);
			break;
		case XML_LT:
			if (ISSPACE(ch)) {
				if (this->nesting) {
					this->state = XML_BEFORE_ATTR;
					if (this->step == XML_STEP_TAG)
						this->type = XML_start_tag_TAG_CH;
					else if (this->step == XML_STEP_NESTED)
						this->type = XML_TAG_CH;
					else if (this->step == XML_STEP_NONE)
						this->type = XML_end_cdata_TAG_CH;
					else
						this->type = XML_add_attr_TAG_CH;
					this->step = XML_STEP_TAG;
					increment_nesting(ch);
					this->count = 0;
				}
				else {
					this->state = XML_IN_CDATA;
					this->type = XML_CDATA_CH;
					ADD_CHAR(this, ch);
				}
			}
			else if (ch == '!') {
				this->state = XML_LT_BANG;
				this->type = XML_noop;
				ADD_CHAR(this, ch);
			}
			else {
				this->state = XML_IN_TAG_NAME;
				if (this->step == XML_STEP_TAG)
					this->type = XML_start_tag_TAG_CH;
				else if (this->step == XML_STEP_NESTED)
					this->type = XML_TAG_CH;
				else if (this->step == XML_STEP_NONE)
					this->type = XML_end_cdata_TAG_CH;
				else
					this->type = XML_add_attr_TAG_CH;
				this->step = XML_STEP_TAG;
				increment_nesting(ch);
				SET_CHAR(this, ch);
			}
			break;
		case XML_LT_BANG:
			if (ch == '-') {
				this->state = XML_LT_BANG_DASH;
				this->type = XML_noop;
			}
			else if (ch == '[') {
				this->state = XML_LT_BANG_SQR;
				this->type = XML_noop;
			}
			else {
				this->state = XML_IN_TAG_NAME;
				if (this->step == XML_STEP_TAG)
					this->type = XML_start_tag_TAG_CH;
				else if (this->step == XML_STEP_NESTED)
					this->type = XML_TAG_CH;
				else if (this->step == XML_STEP_NONE)
					this->type = XML_end_cdata_TAG_CH;
				else
					this->type = XML_add_attr_TAG_CH;
				this->step = XML_STEP_TAG;
				increment_nesting('!');
				SET_CHAR(this, '!');
			}
			ADD_CHAR(this, ch);
			break;
		case XML_LT_BANG_DASH:
			if (ch == '-') {
				this->state = XML_IN_COMMENT;
				if (this->step == XML_STEP_TAG)
					this->type = XML_start_tag_start_comment;
				else if (this->step == XML_STEP_NESTED)
					this->type = XML_start_comment;
				else if (this->step == XML_STEP_NONE)
					this->type = XML_end_cdata_start_comment;
				else
					this->type = XML_add_attr_start_comment;
				increment_nesting(' ');
			}
			else {
				this->state = XML_IN_CDATA;
				this->type = XML_CDATA_CH;
				ADD_CHAR(this, ch);
			}
			break;
		case XML_LT_BANG_SQR:
			if (ISSPACE(ch))
				this->type = XML_noop;
			else if (ch == '[') {
				this->state = XML_BEFORE_ATTR;
				if (this->step == XML_STEP_TAG)
					this->type = XML_start_tag_TAG_CH;
				else if (this->step == XML_STEP_NESTED)
					this->type = XML_TAG_CH;
				else if (this->step == XML_STEP_NONE)
					this->type = XML_end_cdata_TAG_CH;
				else
					this->type = XML_add_attr_TAG_CH;
				this->step = XML_STEP_TAG;
				increment_nesting('[');
				SET_CHAR(this, '!');
				ADD_CHAR(this, '[');
			}
			else {
				this->state = XML_LT_BANG_SQR_IN_NAME;
				this->type = XML_noop;
				SET_CHAR(this, '!');
				ADD_CHAR(this, '[');
				ADD_CHAR(this, ch);
			}
			break;
		case XML_LT_BANG_SQR_IN_NAME:
			if (ISSPACE(ch)) {
				this->state = XML_LT_BANG_SQR_AFTER_NAME;
				this->type = XML_noop;
			}
			else if (ch == '[') {
				if (match_string("![CDATA")) {
					this->state = XML_IN_CDATA_TAG;
					if (this->step == XML_STEP_TAG)
						this->type = XML_start_tag_start_cdata_tag;
					else if (this->step == XML_STEP_NESTED)
						this->type = XML_start_cdata_tag;
					else if (this->step == XML_STEP_NONE)
						this->type = XML_end_cdata_start_cdata_tag;
					else
						this->type = XML_add_attr_start_cdata_tag;
					this->step = XML_STEP_TAG;
					increment_nesting('[');
				}
				else {
					this->state = XML_BEFORE_ATTR;
					if (this->step == XML_STEP_TAG)
						this->type = XML_start_tag_TAG_CH;
					else if (this->step == XML_STEP_NESTED)
						this->type = XML_TAG_CH;
					else if (this->step == XML_STEP_NONE)
						this->type = XML_end_cdata_TAG_CH;
					else
						this->type = XML_add_attr_TAG_CH;
					this->step = XML_STEP_TAG;
					increment_nesting('[');
				}
			}
			else {
				this->type = XML_noop;
				ADD_CHAR(this, ch);
			}
			break;
		case XML_LT_BANG_SQR_AFTER_NAME:
			if (ch == '[') {
				if (match_string("![CDATA")) {
					this->state = XML_IN_CDATA_TAG;
					if (this->step == XML_STEP_TAG)
						this->type = XML_start_tag_start_cdata_tag;
					else if (this->step == XML_STEP_NESTED)
						this->type = XML_start_cdata_tag;
					else if (this->step == XML_STEP_NONE)
						this->type = XML_end_cdata_start_cdata_tag;
					else
						this->type = XML_add_attr_start_cdata_tag;
					increment_nesting('[');
				}
				else {
					this->state = XML_BEFORE_ATTR;
					if (this->step == XML_STEP_TAG)
						this->type = XML_start_tag_TAG_CH;
					else if (this->step == XML_STEP_NESTED)
						this->type = XML_TAG_CH;
					else if (this->step == XML_STEP_NONE)
						this->type = XML_end_cdata_TAG_CH;
					else
						this->type = XML_add_attr_TAG_CH;
					this->step = XML_STEP_TAG;
					increment_nesting('[');
				}
			}
			else
				/* Ignore data until the '['!!! */
				this->type = XML_noop;
			break;
		case XML_IN_TAG_NAME:
			if (ISSPACE(ch)) {
				this->state = XML_BEFORE_ATTR;
				this->type = XML_noop;
			}
			else if (ch == '<') {
				this->state = XML_LT;
				this->type = XML_noop;
			}
			else if (ch == '>') {
				if (this->step == XML_STEP_TAG)
					this->type = XML_start_tag_end_tag(END_TAG_TYPE(this));
				else if (this->step == XML_STEP_NESTED)
					this->type = XML_end_tag(END_TAG_TYPE(this));
				else
					this->type = XML_add_attr_end_tag(END_TAG_TYPE(this));
				this->nesting--;
				if (this->nesting) {
					this->step = XML_STEP_NESTED;
					this->state = XML_BEFORE_ATTR;
				}
				else {
					this->step = XML_STEP_NONE;
					this->state = XML_IN_CDATA;
				}
			}
			else if (ch == '"' || ch == '\'') {
				this->state = XML_QUOTE_BEFORE_VALUE;
				this->quote = ch;
				this->type = XML_noop;
			}
			else if (ch == '/' && (END_TAG_TYPE(this) == XML_OP_1_END_TAG)) {
				this->state = XML_SLASH;
				this->type = XML_noop;
			}
			else if (ch == '?' && (END_TAG_TYPE(this) == XML_OP_1_END_PI_TAG)) {
				this->state = XML_QMARK;
				this->type = XML_noop;
			}
			else if (ch == ']' && (END_TAG_TYPE(this) == XML_OP_1_END_BRACKET_TAG)) {
				this->state = XML_SQR;
				this->type = XML_noop;
			}
			else if (ISSINGLE(ch)) {
				this->state = XML_BEFORE_ATTR;
				if (this->step == XML_STEP_TAG)
					this->type = XML_start_tag_ATTR_CH;
				else if (this->step == XML_STEP_NESTED)
					this->type = XML_ATTR_CH;
				else
					this->type = XML_add_attr_ATTR_CH;
				this->step = XML_STEP_ATTR;
				SET_CHAR(this, ch);
			}
			else {
				this->type = XML_TAG_CH;
				SET_CHAR(this, ch);
			}
			break;
		case XML_BEFORE_ATTR:
			if (ISSPACE(ch))
				this->type = XML_noop;
			else if (ch == '<') {
				this->state = XML_LT;
				this->type = XML_noop;
			}
			else if (ch == '>') {
				if (this->step == XML_STEP_TAG)
					this->type = XML_start_tag_end_tag(END_TAG_TYPE(this));
				else if (this->step == XML_STEP_NESTED)
					this->type = XML_end_tag(END_TAG_TYPE(this));
				else
					this->type = XML_add_attr_end_tag(END_TAG_TYPE(this));
				this->nesting--;
				if (this->nesting) {
					this->step = XML_STEP_NESTED;
					this->state = XML_BEFORE_ATTR;
				}
				else {
					this->step = XML_STEP_NONE;
					this->state = XML_IN_CDATA;
				}
			}
			else if (ch == '"' || ch == '\'') {
				this->state = XML_QUOTE_BEFORE_VALUE;
				this->quote = ch;
				this->type = XML_noop;
			}
			else if (ch == '/' && (END_TAG_TYPE(this) == XML_OP_1_END_TAG)) {
				this->state = XML_SLASH;
				this->type = XML_noop;
			}
			else if (ch == '?' && (END_TAG_TYPE(this) == XML_OP_1_END_PI_TAG)) {
				this->state = XML_QMARK;
				this->type = XML_noop;
			}
			else if (ch == ']' && (END_TAG_TYPE(this) == XML_OP_1_END_BRACKET_TAG)) {
				this->state = XML_SQR;
				this->type = XML_noop;
			}
			else if (ISSINGLE(ch)) {
				if (this->step == XML_STEP_TAG)
					this->type = XML_start_tag_ATTR_CH;
				else if (this->step == XML_STEP_NESTED)
					this->type = XML_ATTR_CH;
				else
					this->type = XML_add_attr_ATTR_CH;
				this->step = XML_STEP_ATTR;
				SET_CHAR(this, ch);
			}
			else {
				this->state = XML_IN_ATTR;
				if (this->step == XML_STEP_TAG)
					this->type = XML_start_tag_ATTR_CH;
				else if (this->step == XML_STEP_NESTED)
					this->type = XML_ATTR_CH;
				else
					this->type = XML_add_attr_ATTR_CH;
				this->step = XML_STEP_ATTR;
				SET_CHAR(this, ch);
			}
			break;
		case XML_IN_ATTR:
			if (ISSPACE(ch)) {
				this->state = XML_BEFORE_EQUAL;
				this->type = XML_noop;
			}
			else if (ch == '<') {
				this->state = XML_LT;
				this->type = XML_noop;
			}
			else if (ch == '>') {
				if (this->step == XML_STEP_TAG)
					this->type = XML_start_tag_end_tag(END_TAG_TYPE(this));
				else if (this->step == XML_STEP_NESTED)
					this->type = XML_end_tag(END_TAG_TYPE(this));
				else
					this->type = XML_add_attr_end_tag(END_TAG_TYPE(this));
				this->nesting--;
				if (this->nesting) {
					this->step = XML_STEP_NESTED;
					this->state = XML_BEFORE_ATTR;
				}
				else {
					this->step = XML_STEP_NONE;
					this->state = XML_IN_CDATA;
				}
			}
			else if (ch == '"' || ch == '\'') {
				this->state = XML_QUOTE_BEFORE_VALUE;
				this->quote = ch;
				this->type = XML_noop;
			}
			else if (ch == '/' && (END_TAG_TYPE(this) == XML_OP_1_END_TAG)) {
				this->state = XML_SLASH;
				this->type = XML_noop;
			}
			else if (ch == '?' && (END_TAG_TYPE(this) == XML_OP_1_END_PI_TAG)) {
				this->state = XML_QMARK;
				this->type = XML_noop;
			}
			else if (ch == ']' && (END_TAG_TYPE(this) == XML_OP_1_END_BRACKET_TAG)) {
				this->state = XML_SQR;
				this->type = XML_noop;
			}
			else if (ISSINGLE(ch)) {
				this->state = XML_BEFORE_ATTR;
				if (this->step == XML_STEP_TAG)
					this->type = XML_start_tag_ATTR_CH;
				else if (this->step == XML_STEP_NESTED)
					this->type = XML_ATTR_CH;
				else
					this->type = XML_add_attr_ATTR_CH;
				this->step = XML_STEP_ATTR;
				SET_CHAR(this, ch);
			}
			else if (ch == '=') {
				this->state = XML_AFTER_EQUAL;
				this->type = XML_noop;
			}
			else {
				this->type = XML_ATTR_CH;
				SET_CHAR(this, ch);
			}
			break;
		case XML_BEFORE_EQUAL:
			if (ISSPACE(ch))
				this->type = XML_noop;
			else if (ch == '<') {
				this->state = XML_LT;
				this->type = XML_noop;
			}
			else if (ch == '>') {
				if (this->step == XML_STEP_TAG)
					this->type = XML_start_tag_end_tag(END_TAG_TYPE(this));
				else if (this->step == XML_STEP_NESTED)
					this->type = XML_end_tag(END_TAG_TYPE(this));
				else
					this->type = XML_add_attr_end_tag(END_TAG_TYPE(this));
				this->nesting--;
				if (this->nesting) {
					this->step = XML_STEP_NESTED;
					this->state = XML_BEFORE_ATTR;
				}
				else {
					this->step = XML_STEP_NONE;
					this->state = XML_IN_CDATA;
				}
			}
			else if (ch == '"' || ch == '\'') {
				this->state = XML_QUOTE_BEFORE_VALUE;
				this->quote = ch;
				this->type = XML_noop;
			}
			else if (ch == '/' && (END_TAG_TYPE(this) == XML_OP_1_END_TAG)) {
				this->state = XML_SLASH;
				this->type = XML_noop;
			}
			else if (ch == '?' && (END_TAG_TYPE(this) == XML_OP_1_END_PI_TAG)) {
				this->state = XML_QMARK;
				this->type = XML_noop;
			}
			else if (ch == ']' && (END_TAG_TYPE(this) == XML_OP_1_END_BRACKET_TAG)) {
				this->state = XML_SQR;
				this->type = XML_noop;
			}
			else if (ISSINGLE(ch)) {
				this->state = XML_BEFORE_ATTR;
				if (this->step == XML_STEP_TAG)
					this->type = XML_start_tag_ATTR_CH;
				else if (this->step == XML_STEP_NESTED)
					this->type = XML_ATTR_CH;
				else
					this->type = XML_add_attr_ATTR_CH;
				this->step = XML_STEP_ATTR;
				SET_CHAR(this, ch);
			}
			else if (ch == '=') {
				this->state = XML_AFTER_EQUAL;
				this->type = XML_noop;
			}
			else {
				this->state = XML_IN_ATTR;
				if (this->step == XML_STEP_TAG)
					this->type = XML_start_tag_ATTR_CH;
				else if (this->step == XML_STEP_NESTED)
					this->type = XML_ATTR_CH;
				else
					this->type = XML_add_attr_ATTR_CH;
				this->step = XML_STEP_ATTR;
				SET_CHAR(this, ch);
			}
			break;
		case XML_AFTER_EQUAL:
			if (ISSPACE(ch)) {
				this->state = XML_AFTER_EQUAL;
				this->type = XML_noop;
			}
			else if (ch == '<') {
				this->state = XML_LT;
				this->type = XML_noop;
			}
			else if (ch == '>') {
				if (this->step == XML_STEP_TAG)
					this->type = XML_start_tag_end_tag(END_TAG_TYPE(this));
				else if (this->step == XML_STEP_NESTED)
					this->type = XML_end_tag(END_TAG_TYPE(this));
				else
					this->type = XML_add_attr_end_tag(END_TAG_TYPE(this));
				this->nesting--;
				if (this->nesting) {
					this->step = XML_STEP_NESTED;
					this->state = XML_BEFORE_ATTR;
				}
				else {
					this->step = XML_STEP_NONE;
					this->state = XML_IN_CDATA;
				}
			}
			else if (ch == '"' || ch == '\'') {
				this->state = XML_QUOTE_BEFORE_VALUE;
				this->quote = ch;
				this->type = XML_noop;
			}
			else if (ch == '/' && (END_TAG_TYPE(this) == XML_OP_1_END_TAG)) {
				this->state = XML_SLASH;
				this->type = XML_noop;
			}
			else if (ch == '?' && (END_TAG_TYPE(this) == XML_OP_1_END_PI_TAG)) {
				this->state = XML_QMARK;
				this->type = XML_noop;
			}
			else if (ch == ']' && (END_TAG_TYPE(this) == XML_OP_1_END_BRACKET_TAG)) {
				this->state = XML_SQR;
				this->type = XML_noop;
			}
			else if (ISSINGLE(ch)) {
				this->state = XML_BEFORE_ATTR;
				if (this->step == XML_STEP_TAG)
					this->type = XML_start_tag_ATTR_CH;
				else if (this->step == XML_STEP_NESTED)
					this->type = XML_ATTR_CH;
				else
					this->type = XML_add_attr_ATTR_CH;
				this->step = XML_STEP_ATTR;
				SET_CHAR(this, ch);
			}
			else {
				this->state = XML_IN_VALUE;
				this->quote = 0;
				if (this->step == XML_STEP_TAG)
					this->type = XML_start_tag_VALUE_CH;
				else if (this->step == XML_STEP_VALUE)
					this->type = XML_add_attr_VALUE_CH;
				else
					this->type = XML_VALUE_CH;
				this->step = XML_STEP_VALUE;
				SET_CHAR(this, ch);
			}
			break;
		case XML_QUOTE_BEFORE_VALUE:
			if (ch == this->quote) {
				this->state = XML_QUOTE_AFTER_VALUE;
				// Empty string:
				if (this->step == XML_STEP_TAG)
					this->type = XML_start_tag_VALUE_CH;
				else if (this->step == XML_STEP_VALUE)
					this->type = XML_add_attr_VALUE_CH;
				else
					this->type = XML_VALUE_CH;
				this->step = XML_STEP_VALUE;
				this->count = 0;
			}
			else {
				this->state = XML_IN_VALUE;
				if (this->step == XML_STEP_TAG)
					this->type = XML_start_tag_VALUE_CH;
				else if (this->step == XML_STEP_VALUE)
					this->type = XML_add_attr_VALUE_CH;
				else
					this->type = XML_VALUE_CH;
				this->step = XML_STEP_VALUE;
				SET_CHAR(this, ch);
			}
			break;
		case XML_IN_VALUE:
			if (this->quote) {
				if (ch == this->quote) {
					this->state = XML_QUOTE_AFTER_VALUE;
					this->type = XML_noop;
				}
				else {
					this->type = XML_VALUE_CH;
					SET_CHAR(this, ch);
				}
			}
			else {
				/* A value without quotes (for HTML!) */
				if (ISSPACE(ch)) {
					this->state = XML_BEFORE_ATTR;
					this->type = XML_noop;
				}
				else if (ch == '<') {
					this->state = XML_LT;
					this->type = XML_noop;
				}
				else if (ch == '>') {
					if (this->step == XML_STEP_TAG)
						this->type = XML_start_tag_end_tag(END_TAG_TYPE(this));
					else if (this->step == XML_STEP_NESTED)
						this->type = XML_end_tag(END_TAG_TYPE(this));
					else
						this->type = XML_add_attr_end_tag(END_TAG_TYPE(this));
					this->nesting--;
					if (this->nesting) {
						this->step = XML_STEP_NESTED;
						this->state = XML_BEFORE_ATTR;
					}
					else {
						this->step = XML_STEP_NONE;
						this->state = XML_IN_CDATA;
					}
				}
				else if (ch == '"' || ch == '\'') {
					this->state = XML_QUOTE_BEFORE_VALUE;
					this->quote = ch;
					this->type = XML_noop;
				}
				else {
					this->type = XML_VALUE_CH;
					SET_CHAR(this, ch);
				}
			}
			break;
		case XML_QUOTE_AFTER_VALUE:
			if (ISSPACE(ch)) {
				this->state = XML_BEFORE_ATTR;
				this->type = XML_noop;
			}
			else if (ch == '<') {
				this->state = XML_LT;
				this->type = XML_noop;
			}
			else if (ch == '>') {
				if (this->step == XML_STEP_TAG)
					this->type = XML_start_tag_end_tag(END_TAG_TYPE(this));
				else if (this->step == XML_STEP_NESTED)
					this->type = XML_end_tag(END_TAG_TYPE(this));
				else
					this->type = XML_add_attr_end_tag(END_TAG_TYPE(this));
				this->nesting--;
				if (this->nesting) {
					this->step = XML_STEP_NESTED;
					this->state = XML_BEFORE_ATTR;
				}
				else {
					this->step = XML_STEP_NONE;
					this->state = XML_IN_CDATA;
				}
			}
			else if (ch == '"' || ch == '\'') {
				this->state = XML_QUOTE_BEFORE_VALUE;
				this->quote = ch;
				this->type = XML_noop;
			}
			else if (ch == '/' && (END_TAG_TYPE(this) == XML_OP_1_END_TAG)) {
				this->state = XML_SLASH;
				this->type = XML_noop;
			}
			else if (ch == '?' && (END_TAG_TYPE(this) == XML_OP_1_END_PI_TAG)) {
				this->state = XML_QMARK;
				this->type = XML_noop;
			}
			else if (ch == ']' && (END_TAG_TYPE(this) == XML_OP_1_END_BRACKET_TAG)) {
				this->state = XML_SQR;
				this->type = XML_noop;
			}
			else if (ISSINGLE(ch)) {
				this->state = XML_BEFORE_ATTR;
				if (this->step == XML_STEP_TAG)
					this->type = XML_start_tag_ATTR_CH;
				else if (this->step == XML_STEP_NESTED)
					this->type = XML_ATTR_CH;
				else
					this->type = XML_add_attr_ATTR_CH;
				this->step = XML_STEP_ATTR;
				SET_CHAR(this, ch);
			}
			else {
				this->state = XML_IN_ATTR;
				if (this->step == XML_STEP_TAG)
					this->type = XML_start_tag_ATTR_CH;
				else if (this->step == XML_STEP_NESTED)
					this->type = XML_ATTR_CH;
				else
					this->type = XML_add_attr_ATTR_CH;
				this->step = XML_STEP_ATTR;
				SET_CHAR(this, ch);
			}
			break;
		case XML_SQR:
			SET_CHAR(this, ']');
			goto cont;
		case XML_SLASH:
			SET_CHAR(this, '/');
			goto cont;
		case XML_QMARK:
			SET_CHAR(this, '?');
			cont:
			if (ISSPACE(ch)) {
				this->state = XML_BEFORE_ATTR;
				if (this->step == XML_STEP_TAG)
					this->type = XML_start_tag_TAG_CH;
				else if (this->step == XML_STEP_NESTED)
					this->type = XML_TAG_CH;
				else if (this->step == XML_STEP_NONE)
					this->type = XML_end_cdata_TAG_CH;
				else
					this->type = XML_add_attr_TAG_CH;
				this->step = XML_STEP_ATTR;
			}
			else if (ch == '<') {
				this->state = XML_LT;
				if (this->step == XML_STEP_TAG)
					this->type = XML_start_tag_TAG_CH;
				else if (this->step == XML_STEP_NESTED)
					this->type = XML_TAG_CH;
				else if (this->step == XML_STEP_NONE)
					this->type = XML_end_cdata_TAG_CH;
				else
					this->type = XML_add_attr_TAG_CH;
				this->step = XML_STEP_TAG;
			}
			else if (ch == '>') {
				if (this->state == XML_SLASH) {
					if (this->step == XML_STEP_TAG)
						this->type = XML_start_tag_end_empty_tag;
					else if (this->step == XML_STEP_NESTED)
						this->type = XML_end_empty_tag;
					else
						this->type = XML_add_attr_end_empty_tag;
				}
				else if (this->state == XML_SQR) {
					if (this->step == XML_STEP_TAG)
						this->type = XML_start_tag_end_tag(XML_OP_1_END_BRACKET_TAG);
					else if (this->step == XML_STEP_NESTED)
						this->type = XML_end_tag(XML_OP_1_END_BRACKET_TAG);
					else
						this->type = XML_add_attr_end_tag(XML_OP_1_END_BRACKET_TAG);
				}
				else {
					if (this->step == XML_STEP_TAG)
						this->type = XML_start_tag_end_pi_tag;
					else if (this->step == XML_STEP_NESTED)
						this->type = XML_end_pi_tag;
					else
						this->type = XML_add_attr_end_pi_tag;
				}
				this->nesting--;
				if (this->nesting) {
					this->step = XML_STEP_NESTED;
					this->state = XML_BEFORE_ATTR;
				}
				else {
					this->step = XML_STEP_NONE;
					this->state = XML_IN_CDATA;
				}
			}
			else if (ch == '"' || ch == '\'') {
				this->state = XML_QUOTE_BEFORE_VALUE;
				this->quote = ch;
				if (this->step == XML_STEP_TAG)
					this->type = XML_start_tag_TAG_CH;
				else if (this->step == XML_STEP_NESTED)
					this->type = XML_TAG_CH;
				else if (this->step == XML_STEP_NONE)
					this->type = XML_end_cdata_TAG_CH;
				else
					this->type = XML_add_attr_TAG_CH;
				this->step = XML_STEP_ATTR;
			}
			else if (ch == '/' && (END_TAG_TYPE(this) == XML_OP_1_END_TAG)) {
				this->state = XML_SLASH;
				if (this->step == XML_STEP_TAG)
					this->type = XML_start_tag_TAG_CH;
				else if (this->step == XML_STEP_NESTED)
					this->type = XML_TAG_CH;
				else if (this->step == XML_STEP_NONE)
					this->type = XML_end_cdata_TAG_CH;
				else
					this->type = XML_add_attr_TAG_CH;
				this->step = XML_STEP_ATTR;
			}
			else if (ch == '?' && (END_TAG_TYPE(this) == XML_OP_1_END_PI_TAG)) {
				this->state = XML_QMARK;
				if (this->step == XML_STEP_TAG)
					this->type = XML_start_tag_TAG_CH;
				else if (this->step == XML_STEP_NESTED)
					this->type = XML_TAG_CH;
				else if (this->step == XML_STEP_NONE)
					this->type = XML_end_cdata_TAG_CH;
				else
					this->type = XML_add_attr_TAG_CH;
				this->step = XML_STEP_ATTR;
			}
			else if (ch == ']' && (END_TAG_TYPE(this) == XML_OP_1_END_BRACKET_TAG)) {
				this->state = XML_SQR;
				if (this->step == XML_STEP_TAG)
					this->type = XML_start_tag_TAG_CH;
				else if (this->step == XML_STEP_NESTED)
					this->type = XML_TAG_CH;
				else if (this->step == XML_STEP_NONE)
					this->type = XML_end_cdata_TAG_CH;
				else
					this->type = XML_add_attr_TAG_CH;
				this->step = XML_STEP_ATTR;
			}
			else if (ISSINGLE(ch)) {
				this->state = XML_BEFORE_ATTR;
				if (this->step == XML_STEP_TAG)
					this->type = XML_start_tag_TAG_CH;
				else if (this->step == XML_STEP_NESTED)
					this->type = XML_TAG_CH;
				else if (this->step == XML_STEP_NONE)
					this->type = XML_end_cdata_TAG_CH;
				else
					this->type = XML_add_attr_TAG_CH;
				this->step = XML_STEP_ATTR;
				ADD_CHAR(this, ch);
			}
			else {
				this->state = XML_IN_ATTR;
				if (this->step == XML_STEP_TAG)
					this->type = XML_start_tag_TAG_CH;
				else if (this->step == XML_STEP_NESTED)
					this->type = XML_TAG_CH;
				else if (this->step == XML_STEP_NONE)
					this->type = XML_end_cdata_TAG_CH;
				else
					this->type = XML_add_attr_TAG_CH;
				this->step = XML_STEP_ATTR;
				ADD_CHAR(this, ch);
			}
			break;
		case XML_IN_COMMENT:
			if (ch == '-') {
				this->state = XML_IN_COMMENT_DASH;
				this->type = XML_noop;
			}
			else
				this->type = XML_COMMENT_CH;
			SET_CHAR(this, ch);
			break;
		case XML_IN_COMMENT_DASH:
			if (ch == '-') {
				this->state = XML_IN_COMMENT_DASH_DASH;
				this->type = XML_noop;
			}
			else {
				this->state = XML_IN_COMMENT;
				this->type = XML_COMMENT_CH;
			}
			ADD_CHAR(this, ch);
			break;
		case XML_IN_COMMENT_DASH_DASH:
			if (ch == '-') {
				this->state = XML_IN_COMMENT_3_DASH;
				this->type = XML_COMMENT_CH;
				SET_CHAR(this, ch);
			}
			else if (ch == '>') {
				this->type = XML_end_comment;
				this->nesting--;
				if (this->nesting) {
					this->step = XML_STEP_NESTED;
					this->state = XML_BEFORE_ATTR;
				}
				else {
					this->step = XML_STEP_NONE;
					this->state = XML_IN_CDATA;
				}
			}
			else {
				this->state = XML_IN_COMMENT;
				this->type = XML_COMMENT_CH;
				ADD_CHAR(this, ch);
			}
			break;
		case XML_IN_COMMENT_3_DASH:
			if (ch == '-') {
				this->type = XML_COMMENT_CH;
				SET_CHAR(this, ch);
			}
			else if (ch == '>') {
				this->type = XML_end_comment;
				this->nesting--;
				if (this->nesting) {
					this->step = XML_STEP_NESTED;
					this->state = XML_BEFORE_ATTR;
				}
				else {
					this->step = XML_STEP_NONE;
					this->state = XML_IN_CDATA;
				}
			}
			else {
				this->state = XML_IN_COMMENT;
				this->type = XML_COMMENT_CH;
				SET_CHAR(this, '-');
				ADD_CHAR(this, '-');
				ADD_CHAR(this, ch);
			}
			break;
		case XML_IN_CDATA_TAG:
			if (ch == ']') {
				this->state = XML_IN_CDATA_TAG_SQR;
				this->type = XML_noop;
			}
			else
				this->type = XML_CDATA_TAG_CH;
			SET_CHAR(this, ch);
			break;
		case XML_IN_CDATA_TAG_SQR:
			if (ch == ']') {
				this->state = XML_IN_CDATA_TAG_SQR_SQR;
				this->type = XML_noop;
			}
			else {
				this->state = XML_IN_CDATA_TAG;
				this->type = XML_CDATA_TAG_CH;
			}
			ADD_CHAR(this, ch);
			break;
		case XML_IN_CDATA_TAG_SQR_SQR:
			if (ch == ']') {
				this->state = XML_IN_CDATA_TAG_3_SQR;
				this->type = XML_CDATA_TAG_CH;
				SET_CHAR(this, ch);
			}
			else if (ch == '>') {
				this->type = XML_end_cdata_tag;
				this->nesting--;
				if (this->nesting) {
					this->step = XML_STEP_NESTED;
					this->state = XML_BEFORE_ATTR;
				}
				else {
					this->step = XML_STEP_NONE;
					this->state = XML_IN_CDATA;
				}
			}
			else {
				this->state = XML_IN_CDATA_TAG;
				this->type = XML_CDATA_TAG_CH;
				ADD_CHAR(this, ch);
			}
			break;
		case XML_IN_CDATA_TAG_3_SQR:
			if (ch == ']') {
				this->type = XML_CDATA_TAG_CH;
				SET_CHAR(this, ch);
			}
			else if (ch == '>') {
				this->type = XML_end_cdata_tag;
				this->nesting--;
				if (this->nesting) {
					this->step = XML_STEP_NESTED;
					this->state = XML_BEFORE_ATTR;
				}
				else {
					this->step = XML_STEP_NONE;
					this->state = XML_IN_CDATA;
				}
			}
			else {
				this->state = XML_IN_CDATA_TAG;
				this->type = XML_CDATA_TAG_CH;
				SET_CHAR(this, ']');
				ADD_CHAR(this, ']');
				ADD_CHAR(this, ch);
			}
			break;
	}
	return(this->type);
}

/* ------------------------------------------------------------------- */
/* CSXMLProcessor */

bool CSXMLProcessor::buildConversionTable()
{
	int32_t i;

	/* By default we don't know how to convert any charset
	 * other tha ISO-1 to unicode!
	 */
	if (strcasecmp(charset, "ISO-8859-1") == 0) {
		for (i=0; i<128; i++)
			conversion_table[i] = (wchar_t) (i + 128);
	}
	else {
		for (i=0; i<128; i++)
			conversion_table[i] = '?';
	}
	return true;
}

// Private use are: E000 - F8FF

int32_t CSXMLProcessor::capture_initializer(wchar_t ch)
/* We capture tag and attribute data for the parsing purposes.
 * The buffers are initialized here (at the lowest level)
 * of processing after parsing.
 */
{
	int32_t op;

	op = parseChar(ch);
	switch (op & XML_OP_1_MASK) {
		case XML_OP_1_START_TAG:
			this->tlength = 0;
			break;
		case XML_OP_1_ADD_ATTR:
			this->nlength = 0;
			this->vlength = 0;
			break;
	}
	return(op);
}

int32_t CSXMLProcessor::entity_translator(wchar_t ch)
/* This function handles entities.
 * Certain entities are translated into UNICODE characters.
 * Strictly speaking, these enties are only recognised by HTML.
 * The few entities that are recognised by XML are first translated
 * into some reserved characters for the parser. This is to ensure
 * that the parser does not recognize them as characters with special
 * meaning! This includes '&', '<' and '>'.
 */
{
	int32_t op;

	op = capture_initializer(ch);
	return(op);
}

/*
 * This function translates the input character stream into UNICODE.
 */
int32_t CSXMLProcessor::charset_transformer(wchar_t ch)
{
	int32_t op;

	// Do transformation according to the charset.
	switch (this->charset_type) {
		case CHARSET_UTF_8:
			if (ch > 127 && ch < 256) {
				uint32_t utf_value;
				uint8_t utf_ch = (uint8_t)ch;

				if ((utf_ch & 0xC0) != 0x80)
					this->utf8_count = 0;
				if ((utf_ch & 0x80) == 0x00)
					this->utf8_length = 1;
				else if ((utf_ch & 0xE0) == 0xC0)
					this->utf8_length = 2;
				else if ((utf_ch & 0xF0) == 0xE0)
					this->utf8_length = 3;
				else if ((utf_ch & 0xF8) == 0xF0)
					this->utf8_length = 4;
				else if ((utf_ch & 0xFC) == 0xF8)
					this->utf8_length = 5;
				else if ((utf_ch & 0xFE) == 0xFC)
					this->utf8_length = 6;
				this->utf8_buffer[this->utf8_count] = (uint32_t) utf_ch;
				this->utf8_count++;
				if (this->utf8_count < this->utf8_length) {
					// I need more bytes!
					setDataType(XML_noop);
					return(XML_noop);
				}
				utf_value = 0;
				switch (this->utf8_length) {
					case 1:
						utf_value = this->utf8_buffer[0] & 0x0000007F;
						break;
					case 2:
						utf_value = ((this->utf8_buffer[0] & 0x0000001F) << 6) |
									(this->utf8_buffer[1] & 0x0000003F);
						if (utf_value < 0x00000080)
							utf_value = '?';
						break;
					case 3:
						utf_value = ((this->utf8_buffer[0] & 0x0000000F) << 12) |
									((this->utf8_buffer[1] & 0x0000003F) << 6) |
									(this->utf8_buffer[2] & 0x0000003F);
						if (utf_value < 0x000000800)
							utf_value = '?';
						break;
					case 4:
						utf_value = ((this->utf8_buffer[0] & 0x00000007) << 18) |
									((this->utf8_buffer[1] & 0x0000003F) << 12) |
									((this->utf8_buffer[2] & 0x0000003F) << 6) |
									(this->utf8_buffer[3] & 0x0000003F);
						if (utf_value < 0x00010000)
							utf_value = '?';
						break;
					case 5:
						utf_value = ((this->utf8_buffer[0] & 0x00000003) << 24) |
									((this->utf8_buffer[1] & 0x0000003F) << 18) |
									((this->utf8_buffer[2] & 0x0000003F) << 12) |
									((this->utf8_buffer[3] & 0x0000003F) << 6) |
									(this->utf8_buffer[4] & 0x0000003F);
						if (utf_value < 0x00200000)
							utf_value = '?';
						break;
					case 6:
						utf_value = ((this->utf8_buffer[0] & 0x00000001) << 30) |
									((this->utf8_buffer[1] & 0x0000003F) << 24) |
									((this->utf8_buffer[2] & 0x0000003F) << 18) |
									((this->utf8_buffer[3] & 0x0000003F) << 12) |
									((this->utf8_buffer[4] & 0x0000003F) << 6) |
									(this->utf8_buffer[5] & 0x0000003F);
						if (utf_value < 0x04000000)
							utf_value = '?';
						break;
				}
				if (utf_value > 0x0000FFFF)
					ch = '?';
				else
					ch = utf_value;
			}
			break;
		case CHARSET_TO_CONVERT_8_BIT:
			if (ch > 127 && ch < 256)
				ch = this->conversion_table[((unsigned char) ch) - 128];
			break;
	}

	op = entity_translator(ch);

	// Determine the characters set:
	switch (op & XML_OP_1_MASK) {
		case XML_OP_1_START_TAG:
			if (strcmp(this->pr_tag, "?xml") == 0)
				this->ip = true;
			else
				this->ip = false;
			break;
		case XML_OP_1_ADD_ATTR:
			if (this->ip) {
				if (strcasecmp(this->pr_name, "encoding") == 0) {
					strcpy(this->charset, this->pr_value);
					if (STRCASESTR(this->charset, "utf-8"))
						this->charset_type = CHARSET_UTF_8;
					else if (STRCASESTR(this->charset, "ucs-2") ||
						STRCASESTR(this->charset, "ucs-4") ||
						STRCASESTR(this->charset, "unicode"))
						this->charset_type = CHARSET_STANDARD;
					else {
						this->charset_type = CHARSET_TO_CONVERT_8_BIT;
						buildConversionTable();
					}
				}
			}
			break;
	}
	return(op);
}

void CSXMLProcessor::appendWCharToString(char *dstr, size_t *dlen, size_t dsize, wchar_t *schars, size_t slen)
{
	for (size_t i=0; i < slen; i++) {
		if (*dlen < dsize-1) {
			if (*schars > 127)
				dstr[*dlen] = '~';
			else
				dstr[*dlen] = (char)*schars;
			(*dlen)++;
			schars++;
			dstr[*dlen] = 0;
		}
	}
}

int32_t CSXMLProcessor::processChar(wchar_t ch)
{
	int32_t op;

	op = charset_transformer(ch);

	/*
	 * Capture output tag and attribute data.
	 * This must be done at the highest level, after
	 * parsing.
	 */
	switch (op & XML_DATA_MASK) {
		case XML_DATA_TAG:
			appendWCharToString(this->pr_tag, &this->tlength, CS_MAX_XML_NAME_SIZE, this->getDataPtr(), this->getDataLen());
			break;
		case XML_DATA_ATTR:
			appendWCharToString(this->pr_name, &this->nlength, CS_MAX_XML_NAME_SIZE, this->getDataPtr(), this->getDataLen());
			break;
		case XML_DATA_VALUE:
			appendWCharToString(this->pr_value, &this->vlength, CS_MAX_XML_NAME_SIZE, this->getDataPtr(), this->getDataLen());
			break;
	}
	return(op);
}

bool CSXMLProcessor::getError(int32_t *err, char **msg)
{
	*err = err_no;
	*msg = err_message;
	return err_no != 0;
}

void CSXMLProcessor::setError(int32_t err, char *msg)
{
	err_no = err;
	if (msg) {
		strncpy(err_message, msg, CS_XML_ERR_MSG_SIZE);
		err_message[CS_XML_ERR_MSG_SIZE-1] = 0;
		return;
	}

	switch (err) {
		case CS_XML_ERR_OUT_OF_MEMORY:
			snprintf(err_message, CS_XML_ERR_MSG_SIZE, "AES parse error- insufficient memory");			
			break;
		case CS_XML_ERR_CHAR_TOO_LARGE:
			snprintf(err_message, CS_XML_ERR_MSG_SIZE, "AES parse error- UNICODE character too large to be encoded as UTF-8");			
			break;
		default:
			snprintf(err_message, CS_XML_ERR_MSG_SIZE, "AES parse error- %s", strerror(err));
			break;
	}
}

void CSXMLProcessor::printError(char *prefix)
{
	printf("%s%s", prefix, err_message);
}

/* ------------------------------------------------------------------- */
/* CSXMLString */

#ifdef DEBUG_ALL
#define EXTRA_SIZE			2
#else
#define EXTRA_SIZE			100
#endif

bool CSXMLString::addChar(char ch, CSXMLProcessor *xml)
{
	char *ptr;

	if (stringLen + 2 > stringSize) {
		if (!(ptr = (char *) realloc(stringPtr, stringLen + 2 + EXTRA_SIZE))) {
			xml->setError(CS_XML_ERR_OUT_OF_MEMORY, NULL);
			return false;
		}
		stringPtr = ptr;
		stringSize = stringLen + 2 + EXTRA_SIZE;
	}
	stringPtr[stringLen] = ch;
	stringPtr[stringLen+1] = 0;
	stringLen++;
	return true;
}

bool CSXMLString::addChars(size_t size, wchar_t *buffer, bool to_lower, CSXMLProcessor *xml)
{
	size_t		i;
	uint32_t	uni_char;
	int32_t			shift;

	for (i=0; i<size; i++) {
		uni_char = (uint32_t) buffer[i];
		
		/* Convertion to lower only done for ASCII! */
		if (to_lower && uni_char <= 127)
			uni_char = (uint32_t) tolower((int32_t) uni_char);

		// Convert to UTF-8!
		if (uni_char <= 0x0000007F) {
			if (!addChar((char) uni_char, xml))
				return false;
			shift = -6;
		}
		else if (uni_char <= 0x000007FF) {
			if (!addChar((char) ((0x000000C0) | ((uni_char >> 6) & 0x0000001F)), xml))
				return false;
			shift = 0;
		}
		else if (uni_char <= 0x00000FFFF) {
			if (!addChar((char) ((0x000000E0) | ((uni_char >> 12) & 0x0000000F)), xml))
				return false;
			shift = 6;
		}
		else if (uni_char <= 0x001FFFFF) {
			if (!addChar((char) ((0x000000F0) | ((uni_char >> 18) & 0x00000007)), xml))
				return false;
			shift = 12;
		}
		else if (uni_char <= 0x003FFFFFF) {
			if (!addChar((char) ((0x000000F0) | ((uni_char >> 24) & 0x00000003)), xml))
				return false;
			shift = 18;
		}
		else if (uni_char <= 0x07FFFFFFF) {
			if (!addChar((char) ((0x000000F0) | ((uni_char >> 30) & 0x00000001)), xml))
				return false;
			shift = 24;
		}
		else {
			xml->setError(CS_XML_ERR_CHAR_TOO_LARGE, NULL);
			return false;
		}

		while (shift >= 0) {
			if (!addChar((char) ((0x00000080) | ((uni_char >> shift) & 0x0000003F)), xml))
				return false;
			shift -= 6;
		}
	}
	return true;
}

bool CSXMLString::addString(const char *string, CSXMLProcessor *xml)
{
	bool ok = true;
	
	while (*string && ok) {
		ok = addChar(*string, xml);
		string++;
	}
	return ok;
}

void CSXMLString::setEmpty()
{
	stringLen = 0;
	if (stringPtr)
		*stringPtr = 0;
}

void CSXMLString::setNull()
{
	free(stringPtr);
	stringPtr = NULL;
	stringLen = 0;
	stringSize = 0;
}

char *CSXMLString::lastComponent()
{
	char *ptr;

	if (stringLen == 0)
		return NULL;

	ptr = stringPtr + stringLen - 1;
	while (ptr > stringPtr && *ptr != '/')
		ptr--;
	return ptr;
}

/* We assume comp begins with a '/' */
char *CSXMLString::findTrailingComponent(const char *comp)
{
	char *ptr, *last_slash;

	if (stringLen == 0)
		return NULL;

	ptr = stringPtr + stringLen - 1;
	last_slash = NULL;

	do {
		/* Find the next '/' */
		while (ptr > stringPtr && *ptr != '/')
			ptr--;
		if (last_slash)
			*last_slash = 0;
		if (strcmp(ptr, comp) == 0) {
			if (last_slash)
				*last_slash = '/';
			return ptr;
		}
		if (last_slash)
			*last_slash = '/';
		last_slash = ptr;
		ptr--;
	}
	while (ptr > stringPtr);
	return NULL;
}

void CSXMLString::truncate(char *ptr)
{
	*ptr = 0;
	stringLen = ptr - stringPtr;
}

/* ------------------------------------------------------------------- */
/* CSXML */

#define IS_XML_CDATA				0
#define IS_XML_CDATA_TAG			1
#define IS_XML_TAG					2
#define IS_XML_CLOSE_TAG			3
#define IS_XML_COMMENT				4
#define IS_XML_DTD					5
#define IS_XML_PI					6
#define IS_XML_PI_XML				7
#define IS_XML_IN_EX				8
#define IS_XML_OPEN_BRACKET			9
#define IS_XML_CLOSE_BRACKET		10

int32_t CSXML::nodeType(char *name)
{
	if (name) {
		switch (*name) {
			case 0:
				return IS_XML_CDATA;
			case '[':
				if (strlen(name) == 1)
					return IS_XML_OPEN_BRACKET;
				break;
			case ']':
				if (strlen(name) == 1)
					return IS_XML_CLOSE_BRACKET;
				break;
			case '/':
				return IS_XML_CLOSE_TAG;
			case '!':
				if (strlen(name) > 1) {
					if (strcasecmp(name, "!--") == 0)
						return IS_XML_COMMENT;
					if (name[1] == '[') {
						if (strcasecmp(name, "![CDATA[") == 0)
							return IS_XML_CDATA_TAG;
						return IS_XML_IN_EX;
					}
				}
				return IS_XML_DTD;
			case '?':
				if (strcasecmp(name, "?xml") == 0)
					return IS_XML_PI_XML;
				return IS_XML_PI;
		}
		return IS_XML_TAG;
	}
	return IS_XML_CDATA;
}

bool CSXML::internalCloseNode(const char *name, bool single)
{
	bool	ok = true;
	char	*ptr;

	if (single) {
		if ((ptr = xml_path.lastComponent())) {
			ok = closeNode(xml_path.stringPtr);
			xml_path.truncate(ptr);
		}
	}
	else if ((ptr = xml_path.findTrailingComponent(name))) {
		/* Close the node that is named above. If the XML is
		 * correct, then the node should be at the top of the
		 * node stack (last element of the path).
		 *
		 * If not found, "ignore" the close.
		 *
		 * If not found on the top of the node stack, then
		 * we close serveral nodes.
		 */
		for (;;) {
			if (!(ptr = xml_path.lastComponent()))
				break;
			if (!(ok = closeNode(xml_path.stringPtr)))
				break;
			if (strcmp(ptr, name) == 0) {
				xml_path.truncate(ptr);
				break;
			}
			xml_path.truncate(ptr);
		}
	}
	return ok;
}

bool CSXML::internalOpenNode(const char *name)
{
	bool ok;

	ok = xml_path.addString("/", this);
	if (!ok)
		return ok;
	ok = xml_path.addString(name, this);
	if (!ok)
		return ok;
	return openNode(this->xml_path.stringPtr, this->xml_value.stringPtr);
}

bool CSXML::parseXML(int32_t my_flags)
{
	wchar_t	ch;
	bool	ok = true;
	int32_t		op;
	int32_t		tagtype;

	this->flags = my_flags;
	ok = xml_path.addChars(0, NULL, false, this);
	if (!ok)
		goto exit;
	ok = xml_name.addChars(0, NULL, false, this);
	if (!ok)
		goto exit;
	ok = xml_value.addChars(0, NULL, false, this);
	if (!ok)
		goto exit;

	ok = getChar(&ch);
	while (ch != CS_XML_EOF_CHAR && ok) {
		op = processChar(ch);
		switch (op & XML_OP_1_MASK) {
			case XML_OP_1_NOOP:
				break;
			case XML_OP_1_END_TAG:
				break;
			case XML_OP_1_END_CLOSE_TAG:
				break;
			case XML_OP_1_END_EMPTY_TAG:
				ok = internalCloseNode("/>", true);
				break;
			case XML_OP_1_END_PI_TAG:
				ok = internalCloseNode("?>", true);
				break;
			case XML_OP_1_END_ENTITY_TAG:
				ok = internalCloseNode(">", true);
				break;
			case XML_OP_1_END_BRACKET_TAG:
				ok = internalCloseNode("]>", true);
				break;
			case XML_OP_1_END_UNKNOWN_TAG:
				ok = internalCloseNode(">", true);
				break;
			case XML_OP_1_START_CDATA_TAG:
				break;
			case XML_OP_1_START_COMMENT:
				break;
			case XML_OP_1_START_TAG:
				if (nodeType(xml_name.stringPtr) == IS_XML_CLOSE_TAG)
					ok = internalCloseNode(xml_name.stringPtr, false);
				else
					ok = internalOpenNode(xml_name.stringPtr);
				xml_name.setEmpty();
				xml_value.setEmpty();
				break;
			case XML_OP_1_ADD_ATTR:
				tagtype = nodeType(xml_name.stringPtr);
				if (tagtype != IS_XML_OPEN_BRACKET && tagtype != IS_XML_CLOSE_BRACKET)
					ok = addAttribute(xml_path.stringPtr, xml_name.stringPtr, xml_value.stringPtr);
				xml_name.setEmpty();
				xml_value.setEmpty();
				break;
			case XML_OP_1_END_CDATA:
				if (xml_value.stringLen || (my_flags & XML_KEEP_EMPTY_CDATA)) {
					ok = internalOpenNode("");
					xml_name.setEmpty();
					xml_value.setEmpty();
					ok = internalCloseNode("", true);
				}
				break;
			case XML_OP_1_END_CDATA_TAG:
				ok = internalOpenNode("![CDATA[");
				xml_name.setEmpty();
				xml_value.setEmpty();
				if (ok)
					ok = internalCloseNode("]]>", true);
				break;
			case XML_OP_1_END_COMMENT:
				ok = internalOpenNode("!--");
				xml_name.setEmpty();
				xml_value.setEmpty();
				if (ok)
					ok = internalCloseNode("-->", true);
				break;
		}
		if (!ok)
			break;
		switch (op & XML_DATA_MASK) {
			case XML_DATA_TAG:
			case XML_DATA_ATTR:
				ok = xml_name.addChars(getDataLen(), getDataPtr(), true, this);
				break;
			case XML_DATA_CDATA:
			case XML_DATA_CDATA_TAG:
			case XML_COMMENT:
			case XML_DATA_VALUE:
				ok = xml_value.addChars(getDataLen(), getDataPtr(), false, this);
				break;
		}
		if (!ok)
			break;
		switch (op & XML_OP_2_MASK) {
			case XML_OP_2_NOOP:
				break;
			case XML_OP_2_END_TAG:
				break;
			case XML_OP_2_END_CLOSE_TAG:
				break;
			case XML_OP_2_END_EMPTY_TAG:
				ok = internalCloseNode("/>", true);
				break;
			case XML_OP_2_END_PI_TAG:
				ok = internalCloseNode("?>", true);
				break;
			case XML_OP_2_END_ENTITY_TAG:
				ok = internalCloseNode(">", true);
				break;
			case XML_OP_2_END_BRACKET_TAG:
				ok = internalCloseNode("]>", true);
				break;
			case XML_OP_2_END_UNKNOWN_TAG:
				ok = internalCloseNode(">", true);
				break;
			case XML_OP_2_START_CDATA_TAG:
				break;
			case XML_OP_2_START_COMMENT:
				break;
		}
		ok = getChar(&ch);
	}

	exit:
	xml_path.setNull();
	xml_name.setNull();
	xml_value.setNull();
	return ok;
}

/* ------------------------------------------------------------------- */
/* CSXMLPrint */

bool CSXMLPrint::openNode(char *path, char *value)
{
	printf("OPEN  %s\n", path);
	if (value && *value)
		printf("      %s\n", value);
	return true;
}

bool CSXMLPrint::closeNode(char *path)
{
	printf("close %s\n", path);
	return true;
}

bool CSXMLPrint::addAttribute(char *path, char *name, char *value)
{
	if (value)
		printf("attr  %s %s=%s\n", path, name, value);
	else
		printf("attr  %s %s\n", path, name);
	return true;
}

/* ------------------------------------------------------------------- */
/* CSXMLBuffer */

bool CSXMLBuffer::parseString(const char *data, int32_t my_flags)
{
	charData = data;
	dataLen = strlen(data);
	dataPos = 0;
	return parseXML(my_flags);
}

bool CSXMLBuffer::parseData(const char *data, size_t len, int32_t my_flags)
{
	charData = data;
	dataLen = len;
	dataPos = 0;
	return parseXML(my_flags);
}

bool CSXMLBuffer::getChar(wchar_t *ch)
{
	if (dataPos == dataLen)
		*ch = CS_XML_EOF_CHAR;
	else {
		*ch = (wchar_t) (unsigned char) charData[dataPos];
		dataPos++;
	}
	return true;
}

/* ------------------------------------------------------------------- */
/* CSXMLFile */

bool CSXMLFile::parseFile(char *file_name, int32_t my_flags)
{
	bool ok;

	if (!(this->file = fopen(file_name, "r"))) {
		setError(errno, NULL);
		return false;
	}
	ok = parseXML(my_flags);
	fclose(this->file);
	return ok;
}

bool CSXMLFile::getChar(wchar_t *ch)
{
	int32_t next_ch;
	
	next_ch = fgetc(file);
	if (next_ch == EOF) {
		if (ferror(file)) {
			setError(errno, NULL);
			return false;
		}
		*ch = CS_XML_EOF_CHAR;
	}
	else
		*ch = (wchar_t) next_ch;
	return true;
}


