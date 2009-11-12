/* Buffer handling functions, including allocation, deallocation, and I/O.

	Copyright (C) 1993-1998 Sebastiano Vigna 
	Copyright (C) 1999-2009 Todd M. Lewis and Sebastiano Vigna

	This file is part of ne, the nice editor.

	This program is free software; you can redistribute it and/or modify it
	under the terms of the GNU General Public License as published by the
	Free Software Foundation; either version 2, or (at your option) any
	later version.
	
	This program is distributed in the hope that it will be useful, but
	WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
	General Public License for more details.
	
	You should have received a copy of the GNU General Public License along
	with this program; see the file COPYING.  If not, write to the Free
	Software Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
	02111-1307, USA.  */


#include "ne.h" 

/* The standard pool allocation dimension. */

#define STD_POOL_SIZE (16 * 1024)

/* The standard line descriptor pool allocation dimension (in lines). */

#define STD_LINE_DESC_POOL_SIZE (512)

/* The amount by which we increment the first pool dimension, with respect
to the size of the given file. */

#define STANDARD_INCREMENT (8 * 1024)

/* The number of lines by which we increment the first line descriptor
pool dimension, with respect to the number of lines of the given file. */

#define STANDARD_LINE_INCREMENT (256)

/* The size of the space array. Batch printing of spaces happens in blocks of
	this size. */

#define MAX_STACK_SPACES (256)

/* The length of the block used in order to optimize saves. */

#define SAVE_BLOCK_LEN   (64 * 1024 - 1)


/* Detects (heuristically) the encoding of a buffer. */

encoding_type detect_buffer_encoding(const buffer * const b) {
	line_desc *ld = (line_desc *)b->line_desc_list.head, *next;
	encoding_type encoding = ENC_ASCII, e;

	while(next = (line_desc *)ld->ld_node.next) {
		e = detect_encoding(ld->line, ld->line_len);
		if (e != ENC_ASCII) {
			if (encoding == ENC_ASCII) encoding = e;
			if (e == ENC_8_BIT) encoding = ENC_8_BIT;
		}
		ld = next;
	}

	return encoding;
}

/* These functions allocate and deallocate character pools. The size of the
pool is the number of characters, and it is forced to be at least
STD_POOL_SIZE. */


char_pool *alloc_char_pool(int size) {

	char_pool *cp;

	if (size < STD_POOL_SIZE) size = STD_POOL_SIZE;

	if (cp = calloc(1, sizeof(char_pool))) {
		if (cp->pool = calloc(sizeof(char), size)) {
			cp->size = size;
			return cp;
		}
		free(cp);
	}
	return NULL;
}


void free_char_pool(char_pool * const cp) {

	if (cp == NULL) return;

	free(cp->pool);
	free(cp);
}



/* Given a pointer in a character pool and a buffer, this function returns the
	respective pool. It can return NULL if the pointer wasn't in any pool, but
	this condition denotes a severe malfunctioning. */

char_pool *get_char_pool(buffer * const b, unsigned char * const p) {

	char_pool *cp;

	cp = (char_pool *)b->char_pool_list.head;

	while(cp->cp_node.next) {
		assert_char_pool(cp);

		if (p >= cp->pool && p < cp->pool + cp->size) return cp;
		cp = (char_pool *)cp->cp_node.next;
	}

	assert(FALSE);

	return NULL;
}




/* These functions allocate and deallocate line descriptor pools. The size of
	the pool is the number of lines, and is forced to be at least
	STD_LINE_DESC_POOL_SIZE. */


line_desc_pool *alloc_line_desc_pool(int pool_size) {

	line_desc_pool *ldp;

	if (pool_size < STD_LINE_DESC_POOL_SIZE) pool_size = STD_LINE_DESC_POOL_SIZE;

	if (ldp = calloc(1, sizeof(line_desc_pool))) {
		if (ldp->pool = calloc(pool_size, do_syntax ? sizeof(line_desc) : sizeof(no_syntax_line_desc))) {
			int i;

			ldp->size = pool_size;
			new_list(&ldp->free_list);
			for(i = 0; i < pool_size; i++) 
				if (do_syntax) add_tail(&ldp->free_list, &ldp->pool[i].ld_node);
				else add_tail(&ldp->free_list, &((no_syntax_line_desc *)ldp->pool)[i].ld_node);
			return ldp;
		}
		free(ldp);
	}

	return NULL;

}

void free_line_desc_pool(line_desc_pool * const ldp) {

	if (ldp == NULL) return;

	assert_line_desc_pool(ldp);

	free(ldp->pool);
	free(ldp);
}



/* These functions allocate and deallocate a buffer. Note that on allocation
we have to initialize the list pointers, and on dellocation we have to free
all the lists. Moreover, on allocation a buffer pointer can be passed so
that the new buffer can inherit various user flags. */

buffer *alloc_buffer(const buffer * const cur_b) {

	buffer *b;

	if (b = calloc(1, sizeof(buffer))) {

		new_list(&b->line_desc_pool_list);
		new_list(&b->line_desc_list);
		new_list(&b->char_pool_list);
		
		b->cur_macro = alloc_char_stream(0);
		b->opt.tab_size = 8;

		b->opt.status_bar     =
		b->opt.insert         =
		b->opt.verbose_macros =
		b->opt.do_undo        =
		b->opt.auto_prefs     = 1;

		b->opt.utf8auto = io_utf8;

		b->attr_len = -1;

		if (cur_b) {

			b->opt.cur_clip       = cur_b->opt.cur_clip;
			b->opt.tab_size       = cur_b->opt.tab_size;
			b->opt.right_margin   = cur_b->opt.right_margin;

			b->opt.free_form      = cur_b->opt.free_form;
			b->opt.status_bar     = cur_b->opt.status_bar;
			b->opt.hex_code       = cur_b->opt.hex_code;
			b->opt.fast_gui       = cur_b->opt.fast_gui;
			b->opt.word_wrap      = cur_b->opt.word_wrap;
			b->opt.auto_indent    = cur_b->opt.auto_indent;
			b->opt.preserve_cr    = cur_b->opt.preserve_cr;

			b->opt.verbose_macros = cur_b->opt.verbose_macros;
			b->opt.do_undo        = cur_b->opt.do_undo;
			b->opt.auto_prefs     = cur_b->opt.auto_prefs;
			b->opt.no_file_req    = cur_b->opt.no_file_req;

			b->opt.case_search    = cur_b->opt.case_search;
			b->opt.binary         = cur_b->opt.binary;
			b->opt.utf8auto       = cur_b->opt.utf8auto;
			b->opt.visual_bell    = cur_b->opt.visual_bell;

		}
		/* This leaves out onlyopt.read_only and opt.search_back, which are
		 implicitly set to 0 by the calloc(). */
		return b;
	}

	return NULL;
}

/* This function is useful when resetting a buffer, but not really
destroying it. Since it modifies some lists, it cannot be interrupted
from a signal. Note that the search, replace and command_line strings are
not cleared. */

void free_buffer_contents(buffer * const b) {

	if (!b) return;

	block_signals();

	free_list(&b->line_desc_pool_list, free_line_desc_pool);
	free_list(&b->char_pool_list, free_char_pool);
	new_list(&b->line_desc_list);
	b->cur_line_desc = b->top_line_desc = NULL;

	b->allocated_chars = b->free_chars = 0;
	b->is_CRLF = FALSE;
	b->encoding = ENC_ASCII;
	b->bookmark_mask = 0;
        
	free_char_stream(b->last_deleted);
	b->last_deleted = NULL;

	free(b->filename);
	b->filename = NULL;

	reset_undo_buffer(&b->undo);
	b->is_modified = b->marking = b->recording = b->x_wanted = 0;

	release_signals();
}


/* Removes all data in a buffer, but leaves in the current macro, the search,
	replace and command_line strings, and an empty line. */

void clear_buffer(buffer * const b) {

	line_desc *ld;

	if (!b) return;

	block_signals();

	free_buffer_contents(b);

	ld = alloc_line_desc(b);
	add_head(&b->line_desc_list, &ld->ld_node);
	if (do_syntax) {
		ld->highlight_state.state = 0;
		ld->highlight_state.stack = NULL;
		ld->highlight_state.saved_s[0] = 0;
	}

	b->num_lines = 1;
	reset_position_to_sof(b);

	assert_buffer(b);

	release_signals();
}


/* Frees all the data associated to a buffer. */

void free_buffer(buffer * const b) {

	if (b == NULL) return;

	assert_buffer(b);

	free_buffer_contents(b);

	free_char_stream(b->cur_macro);

	free(b->find_string);
	free(b->replace_string);
	free(b->command_line);
	if (b->attr_buf) free(b->attr_buf);
	free(b);
}


/* Computes how many characters have been "lost" in a buffer, that is, how many
	free characters lie inside the first and last used characters of the
	character pools. This characters can only be allocated by
	alloc_chars_around(). */

int calc_lost_chars(const buffer * const b) {

	int n = 0;

	char_pool *cp = (char_pool *)b->char_pool_list.head;

	while(cp->cp_node.next) {

		n += cp->size - (cp->last_used - cp->first_used + 1);
		cp = (char_pool *)cp->cp_node.next;
	}

	return b->free_chars - n;
}


/* Returns the nth buffer in the global buffer list, or NULL if less than n
	buffers are available. */

buffer *get_nth_buffer(int n) {

	buffer *b = (buffer *)buffers.head;

	while(b->b_node.next) {
		if (!n--) return b;
		b = (buffer *)b->b_node.next;
	}
	return NULL;
}



/* Returns a buffer, given its name (i.e., the name of the file it
	contains). Note that file_part() is applied *both* to the string passed
	*and* to the buffer names, so that the path is immaterial. */

buffer *get_buffer_named(const char *p) {

	buffer *b = (buffer *)buffers.head;

	if (!p) return NULL;

	p = file_part((char *)p);

	while(b->b_node.next) {
		if (b->filename && !strcmp(file_part(b->filename), p)) return b;
		b = (buffer *)b->b_node.next;
	}

	return NULL;
}



/* Returns TRUE if any of the buffers has been modified since the last save. */

int modified_buffers(void) {

	buffer *b = (buffer *)buffers.head;

	while(b->b_node.next) {
		if (b->is_modified) return TRUE;
		b = (buffer *)b->b_node.next;
	}
	return FALSE;
}



/* Saves all buffers which have been modified since the last save. Returns an
	error if a save is unsuccessful, or if a buffer has no name. */

int save_all_modified_buffers(void) {

	buffer *b = (buffer *)buffers.head;

	while(b->b_node.next) {
		if (b->is_modified)
			if (save_buffer_to_file(b, NULL)) return ERROR;
		b = (buffer *)b->b_node.next;
	}
	return 0;
}



/* Now we have the much more sophisticated allocation functions which create
	small elements such as lines and line descriptors. The strategy is rather
	complex. All the operations are in the context of a given buffer. Most of
	these functions are protected internally against being interrupted by
	signals, since auto_save could die miserably because of the inconsistent
	state of a list. */


/* Allocates a line descriptor from the pools available in the given buffer. A
	new pool is allocated and linked if necessary. New line descriptors are
	created with an invalid syntax state, so they will always force an update. */

line_desc *alloc_line_desc(buffer * const b) {
	
	
	line_desc_pool *ldp;
	line_desc *ld;

	ldp = (line_desc_pool *)b->line_desc_pool_list.head;

	block_signals();

	while(ldp->ldp_node.next) {

		assert_line_desc_pool(ldp);

		if (ldp->free_list.head->next) {

			ld = (line_desc *)ldp->free_list.head;

			rem(&ld->ld_node);

			if (!ldp->free_list.head->next) {
				rem(&ldp->ldp_node);
				add_tail(&b->line_desc_pool_list, &ldp->ldp_node);
			}

			ldp->allocated_items++;

			ld->line = NULL;
			ld->line_len = 0;
			if (do_syntax) ld->highlight_state.state = -1;
			release_signals();
			return ld;
		}
		ldp = (line_desc_pool *)ldp->ldp_node.next;
	}

	/* No chances, all pools are full. Let's allocate a new one,
	using the standard pool size, and let's put it at the start
	of the list, so that it is always scanned first. */

	if (ldp = alloc_line_desc_pool(0)) {
		add_head(&b->line_desc_pool_list, &ldp->ldp_node);
		rem(&(ld = (line_desc *)ldp->free_list.head)->ld_node);
		ldp->allocated_items = 1;
		if (do_syntax) ld->highlight_state.state = -1;
		release_signals();
		return ld;
	}

	release_signals();
	return NULL;
}



/* Frees a line descriptor, (and the line descriptor pool containing it, should
	it become empty). */

void free_line_desc(buffer * const b, line_desc * const ld) {

	line_desc_pool *ldp;

	ldp = (line_desc_pool *)b->line_desc_pool_list.head;

	assert(ldp != NULL);

	/* We scan the pool list in order to find where the given
	line descriptor lives. */

	while(ldp->ldp_node.next) {
		assert_line_desc_pool(ldp);

		if (ld >= ldp->pool && (do_syntax && ld < ldp->pool + ldp->size || !do_syntax && ld < (line_desc*)((no_syntax_line_desc *)ldp->pool + ldp->size))) break;
		ldp = (line_desc_pool *)ldp->ldp_node.next;
	}

	assert(ldp->ldp_node.next != NULL);

	block_signals();

	add_head(&ldp->free_list, &ld->ld_node);

	if (--ldp->allocated_items == 0) {
		rem(&ldp->ldp_node);
		free_line_desc_pool(ldp);
	}

	release_signals();
}


/* Allocates len characters from the character pools of the
given buffer. If necessary, a new pool is allocated. */

unsigned char *alloc_chars(buffer * const b, const int len) {

	char_pool *cp;

	if (!len || !b) return NULL;

	assert_buffer(b);

	cp = (char_pool *)b->char_pool_list.head;

	assert(cp != NULL);

	block_signals();

	while(cp->cp_node.next) {
		assert_char_pool(cp);

		/* We try to allocate before the first used character,
		or after the last used character. If we succeed with a
		pool which is not the head of the list, we move it to
		the head in order to optimize the next try. */

		if (cp->first_used >= len) {

			cp->first_used -= len;
			b->free_chars -= len;

			if (cp != (char_pool *)b->char_pool_list.head) {
				rem(&cp->cp_node);
				add_head(&b->char_pool_list, &cp->cp_node);
			}

			release_signals();
			return cp->pool + cp->first_used;
		}
		else if (cp->size - cp->last_used > len) {

			cp->last_used += len;
			b->free_chars -= len;

			if (cp != (char_pool *)b->char_pool_list.head) {
				rem(&cp->cp_node);
				add_head(&b->char_pool_list, &cp->cp_node);
			}

			release_signals();
			return cp->pool + cp->last_used - len + 1;
		}
		cp = (char_pool *)cp->cp_node.next;
	}

	/* If no free space has been found, we allocate a new pool which is guaranteed
	to contain at least len characters. The pool is added to the head of the list. */

	if (cp = alloc_char_pool(len)) {
		add_head(&b->char_pool_list, &cp->cp_node);
		cp->last_used = len - 1;

		b->allocated_chars += cp->size;
		b->free_chars += cp->size - len;

		release_signals();
		return cp->pool;
	}

	release_signals();
	return NULL;
}



/* This function is very important, since it embeds all the philosophy behind
	ne's character pool management. It performs an allocation *locally*, that
	is, it tries to see if there are enough free characters around the line
	pointed to by a line descriptor by looking at non-nullness of surrounding
	characters (if a character is set to 0, it is free). First the characters
	after the line are checked, then the characters before (this can be reversed
	via the check_first_before flag). The number of characters available *after*
	the line is returned, or ERROR if the allocation failed. The caller can
	recover the characters available before the line since he knows the length
	of the allocation. Note that it is *only* through this function that the
	"lost" characters can be allocated, but being editing a local activity, this
	is what happens usually. */


int alloc_chars_around(buffer * const b, line_desc * const ld, const int n, const int check_first_before) {

	unsigned char *before, *after;
	char_pool *cp;

	assert(ld->line != NULL);

	cp = get_char_pool(b, ld->line);

	assert_char_pool(cp);

	block_signals();

	before = ld->line - 1;
	after = ld->line + ld->line_len;

	if (check_first_before) {
		while(before >= cp->pool && !*before && (ld->line - 1) - before < n)
			before--;
		while(after < cp->pool + cp->size && !*after && (after - (ld->line + ld->line_len)) + ((ld->line - 1) - before)<n)
			after++;
	}
	else {
		while(after < cp->pool + cp->size && !*after && after - (ld->line + ld->line_len)<n)
			after++;
		while(before >= cp->pool && !*before && (after - (ld->line + ld->line_len)) + ((ld->line - 1) - before)<n)
			before--;
	}

	assert(((ld->line - 1) - before) + (after - (ld->line + ld->line_len)) <= n);
	assert(((ld->line - 1) - before) + (after - (ld->line + ld->line_len)) >= 0);

	if (((ld->line - 1) - before) + (after - (ld->line + ld->line_len)) == n) {
		if (cp->pool + cp->first_used == ld->line) cp->first_used = (before + 1) - cp->pool;
		if (cp->pool + cp->last_used == ld->line + ld->line_len - 1) cp->last_used = (after - 1) - cp->pool;
		b->free_chars -= n;

		release_signals();
		return after - (ld->line + ld->line_len);
	}

	release_signals();
	return ERROR;
}



/* Frees a block of len characters pointed to by p. If the char pool containing
	the block becomes completely free, it is removed from the list. */

void free_chars(buffer *const b, unsigned char *const p, const int len) {

	char_pool *cp;

	if (!b || !p || !len) return;

   cp = get_char_pool(b, p);

   assert_char_pool(cp);

   assert(*p);
   assert(p[len - 1]);

	block_signals();

   memset(p, 0, len);
   b->free_chars += len;

	if (p == &cp->pool[cp->first_used]) while(cp->first_used <= cp->last_used && !cp->pool[cp->first_used]) cp->first_used++;
	if (p + len - 1 == &cp->pool[cp->last_used]) while(!cp->pool[cp->last_used] && cp->first_used <= cp->last_used) cp->last_used--;

	if (cp->last_used < cp->first_used) {
		rem(&cp->cp_node);
		b->allocated_chars -= cp->size;
		b->free_chars -= cp->size;
		free_char_pool(cp);
		release_signals();
		return;
	}

	assert_char_pool(cp);
	release_signals();
}


/* The following functions represent the only legal way of modifying a
	buffer. They are all based on insert_stream and delete_stream (except for
	the I/O functions). A stream is a sequence of NULL-terminated strings. The
	semantics associated is that each string is a separate line terminated by a
	line feed, *except for the last one*. Thus, a NULL-terminated string is a
	line with no linefeed. All the functions accept a position specified via a
	line descriptor and a position (which is the offset to be applied to the
	line pointer of the line descriptor). Also the line number is usually
	supplied, since it is necessary for recording the operation in the undo
	buffer. */


/* Inserts a line at the current position. The effect is obtained by inserting
	a stream containing one NULL. */


int insert_one_line(buffer * const b, line_desc * const ld, const int line, const int pos) {	
	return insert_stream(b, ld, line, pos, "", 1);
}


/* Deletes a whole line, putting it in the temporary line buffer used by the
	UndelLine command. */

int delete_one_line(buffer * const b, line_desc * const ld, const int line) {
	int error;

	assert_line_desc(ld, b->encoding);
	assert_buffer(b);

	block_signals();

	if (ld->line_len && (b->last_deleted = reset_stream(b->last_deleted))) add_to_stream(b->last_deleted, ld->line, ld->line_len);

	/* We delete a line by delete_stream()ing its length plus one. However, if
		we are on the last line of text, there is no terminating line feed. */

	error = delete_stream(b, ld, line, 0, ld->line_len + (ld->ld_node.next->next ? 1 : 0));
	release_signals();

	return error;
}




/* Undeletes the last deleted line, using the last_deleted stream. */

int undelete_line(buffer * const b) {
	line_desc *ld = b->cur_line_desc;
	if (!b->last_deleted) return ERROR;
	start_undo_chain(b);
	if (b->cur_pos > ld->line_len) 
		insert_spaces(b, ld, b->cur_line, ld->line_len, b->win_x + b->cur_x - calc_width(ld, ld->line_len, b->opt.tab_size, b->encoding));

	insert_one_line(b, ld, b->cur_line, b->cur_pos);
	insert_stream(b, ld, b->cur_line, b->cur_pos, b->last_deleted->stream, b->last_deleted->len);
	end_undo_chain(b);
	return OK;
}



/* Deletes a line up to its end. */

void delete_to_eol(buffer * const b, line_desc * const ld, const int line, const int pos) {

	if (!ld || pos >= ld->line_len) return;
	delete_stream(b, ld, line, pos, ld->line_len - pos);
}



/* Inserts a stream in a line at a given position.  The position has to be
	smaller or equal to the line length. Since the stream can contain many
	lines, this function can be used for manipulating all insertions. It also
	record the inverse operation in the undo buffer if b->opt.do_undo is
	TRUE. */

int insert_stream(buffer * const b, line_desc * ld, int line, int pos, const unsigned char * const stream, const int stream_len) {

	int i, len, mask;
	unsigned char *p;
	const unsigned char *s = stream;

	if (!b || !ld || !stream || stream_len < 1 || pos > ld->line_len) return ERROR;

	assert_line_desc(ld, b->encoding);
	assert_buffer(b);

	block_signals();

	if (b->opt.do_undo && !(b->undoing || b->redoing)) {
		int error = add_undo_step(b, line, pos, -stream_len);
		if (error) {
			release_signals();
			return error;
		}
	}

	while(s - stream < stream_len) {

		if (len = strnlen_ne(s, stream_len - (s - stream))) {

			/* First case; there is no character allocated on this line. We
			have to freshly allocate the line. */

			if (!ld->line) {
				if (ld->line = alloc_chars(b, len)) {
					memcpy(ld->line, s, len);
					ld->line_len = len;
				}
				else {
					release_signals();
					return OUT_OF_MEMORY;
				}
			}


			/* Second case. There are not enough characters around ld->line. Note
			that the value of the check_first_before parameter depends on
			the position at which the insertion will be done, and it is chosen
			in such a way to minimize the number of characters to move. */

			else if ((i = alloc_chars_around(b, ld, len, pos < ld->line_len / 2))<0) {
				if (p = alloc_chars(b, ld->line_len + len)) {
					memcpy(p, ld->line, pos);
					memcpy(&p[pos], s, len);
					memcpy(&p[pos + len], ld->line + pos, ld->line_len - pos);
					free_chars(b, ld->line, ld->line_len);
					ld->line = p;
					ld->line_len += len;
				}
				else {
					release_signals();
					return OUT_OF_MEMORY;
				}
			}


			/* Third case. There are enough free characters around ld->line. */

			else {
				if (len - i) memmove(ld->line - (len - i), ld->line, pos);
				if (i) memmove(ld->line + pos + i, ld->line + pos, ld->line_len - pos);
				memcpy(ld->line - (len - i) + pos, s, len);

				ld->line -= (len - i);
				ld->line_len += len;
			}
			b->is_modified = 1;
			
			/* We just inserted len chars at (line,pos); adjust bookmarks and mark accordingly. */
			if (b->marking && b->block_start_line == line && b->block_start_col > pos) b->block_start_col += len;

			for (i=0, mask=b->bookmark_mask;  mask;  i++, mask >>= 1) 
				if ((mask & 1) && b->bookmark[i].line == line && b->bookmark[i].pos > pos) b->bookmark[i].pos += len;
		}

		/* If the string we have inserted has a NULL at the end, we create a new
			line under the current one and set ld to point to it. */

		if (len + (s - stream) < stream_len) {
			line_desc *new_ld;

			if (new_ld = alloc_line_desc(b)) {

				add(&new_ld->ld_node, &ld->ld_node);
				b->num_lines++;

				if (pos + len < ld->line_len) {
					new_ld->line_len = ld->line_len - pos - len;
					new_ld->line = &ld->line[pos + len];
					ld->line_len = pos + len;
					if (pos + len == 0) ld->line = NULL;
				}

				b->is_modified = 1;
				ld = new_ld;
				
				/* We just inserted a line break at (line,pos);
				   adjust the buffer bookmarks and mark accordingly. */
				if (b->marking) {
					if (b->block_start_line == line && b->block_start_col > pos) {
						b->block_start_col -= pos;
						b->block_start_line++;
					}
					else if (b->block_start_line > line) b->block_start_line++;
				}
				for (i=0, mask=b->bookmark_mask;  mask;  i++, mask >>= 1) {
					if (mask & 1) {
						if (b->bookmark[i].line == line && b->bookmark[i].pos > pos) {
							b->bookmark[i].pos -= pos;
							b->bookmark[i].line++;
						}
						else if (b->bookmark[i].line > line) b->bookmark[i].line++;
					}
				}
				pos = 0;
				line++;
			}
			else {
				release_signals();
				return OUT_OF_MEMORY;
			}
		}
		
		
		

		s += len + 1;
	}

	release_signals();
	return OK;
}



/* Inserts a single ISO 10646 character (it creates, if necessary, a suitable
	temporary stream). The character must be compatible with the current buffer
	encoding. */


int insert_one_char(buffer * const b, line_desc * const ld, const int line, const int pos, const int c) {

	
	static unsigned char t[8];

	assert(b->encoding == ENC_8_BIT || b->encoding == ENC_UTF8 || c <= 127);
	assert(b->encoding == ENC_UTF8 || c <= 255);
	assert(c != 0);

	if (b->encoding == ENC_UTF8) t[utf8str(c, t)] = 0;
	else t[0] = c,	t[1] = 0;

	return insert_stream(b, ld, line, pos, t, strlen(t));
}



/* Inserts a number of spaces. */

int insert_spaces(buffer * const b, line_desc * const ld, const int line, const int pos, int n) {
	
	
	
	static unsigned char spaces[MAX_STACK_SPACES];
	int result = OK, i;

	if (!spaces[0]) memset(spaces, ' ', sizeof spaces);

	while(result == OK && n > 0) {
		i = min(n, MAX_STACK_SPACES);
		result = insert_stream(b, ld, line, pos, spaces, i);
		n -= i;
	}

	assert(result != OK || n == 0);

	return result;
}



/* Deletes a stream of len bytes, that is, deletes len bytes from the given
	position, counting line feeds as a byte. The operation is recorded in the
	undo buffer. */

int delete_stream(buffer * const b, line_desc * const ld, const int line, const int pos, int len) {

	int n, m, mask;

	assert_buffer(b);
	assert_line_desc(ld, b->encoding);

	/* If we are in no man's land, we return. */

	if (!b || !ld || !len || pos > ld->line_len || pos == ld->line_len && !ld->ld_node.next->next) return ERROR;

	block_signals();

	if (b->opt.do_undo && !(b->undoing || b->redoing)) {
		int error = add_undo_step(b, line, pos, len);
		if (error) {
			release_signals();
			return error;
		}
	}

	while(len) {

		/* First case: we are just on the end of a line. We join the current
		line with the following one (if it's there of course). If, however,
		the current line is empty, we rather remove it. The only difference
		is in the resulting syntax state. */

		if (pos == ld->line_len) {
			unsigned char *p;
			line_desc *next_ld = (line_desc *)ld->ld_node.next;

			assert(next_ld != NULL);
			assert_line_desc(next_ld, b->encoding);

			/* We're about to join line+1 to line; adjust mark and bookmarks accordingly. */
			if (b->marking) {
				if (b->block_start_line == line+1) {
					b->block_start_line--;
					b->block_start_col += ld->line_len;
				}
				else if (b->block_start_line > line) b->block_start_line--;
			}
			for (n=0, mask=b->bookmark_mask;  mask;  n++, mask >>= 1) {
				if (mask & 1) {
					if (b->bookmark[n].line == line+1) {
						b->bookmark[n].line--;
						b->bookmark[n].pos += ld->line_len;
					}
					else if (b->bookmark[n].line > line) b->bookmark[n].line--;
				}
			}
			
			/* If one of the lines is empty, or their contents are adjacent,
				we either do nothing or simply set a pointer. */

			if (!ld->line || !next_ld->line || ld->line + ld->line_len == next_ld->line) {
				if (!ld->line) ld->line = next_ld->line;
			}
			else if ((n = alloc_chars_around(b, ld, next_ld->line_len, FALSE))<0 && (m = alloc_chars_around(b, next_ld, ld->line_len, TRUE))<0) {
				/* We try to allocate characters around one line or the other
					one; if we fail, we allocate enough space for both lines elsewhere. */

				if (p = alloc_chars(b, ld->line_len + next_ld->line_len)) {

					memcpy(p, ld->line, ld->line_len);
					memcpy(p + ld->line_len, next_ld->line, next_ld->line_len);

					free_chars(b, ld->line, ld->line_len);
					free_chars(b, next_ld->line, next_ld->line_len);

					ld->line = p;
				}
				else {
					release_signals();
					return OUT_OF_MEMORY;
				}
			}

			/* In case one of the alloc_chars_around succeeds, we have just to
			move the lines in the right place. */

			else if (n >= 0) {
				if (n < next_ld->line_len) memmove(ld->line + (n - next_ld->line_len), ld->line, ld->line_len);
				ld->line += (n - next_ld->line_len);
				memcpy(ld->line + ld->line_len, next_ld->line, next_ld->line_len);

				free_chars(b, next_ld->line, next_ld->line_len);
			}
			else {
				if (m) memmove(next_ld->line + m, next_ld->line, next_ld->line_len);
				next_ld->line += m;
				memcpy(next_ld->line - ld->line_len, ld->line, ld->line_len);

				free_chars(b, ld->line, ld->line_len);

				ld->line = next_ld->line - ld->line_len;
			}

			ld->line_len += next_ld->line_len;
			b->num_lines--;

			rem(&next_ld->ld_node);
			free_line_desc(b, next_ld);

			len--;
			if (!b->redoing) {
				if (b->undoing) add_to_stream(&b->undo.redo, "", 1);
				else if (b->opt.do_undo) add_to_undo_stream(&b->undo, "", 1);
			}
		}

		/* Second case: we are inside a line. We delete len characters or, if
			there are less then len characters to delete, we delete up to the end
			of the line. In the latter case, we simply set the line length and
			free the corresponding characters.  Otherwise, the number of characters to
			move is minimized. */

		else {
			n = len > ld->line_len - pos ? ld->line_len - pos : len;
			
			/* We're about to erase n chars at (line,pos); adjust mark and bookmarks accordingly. */
			if (b->marking)
				if (b->block_start_line == line)
					if (b->block_start_col >= pos)
						if (b->block_start_col < pos + n)
							b->block_start_col = pos;
						else
							b->block_start_col -= n;
			for (m=0, mask=b->bookmark_mask;  mask;  m++, mask>>=1) {
				if (mask & 1) {
					if (b->bookmark[m].line == line)
						if (b->bookmark[m].pos >= pos)
							if (b->bookmark[m].pos < pos + n) b->bookmark[m].pos = pos;
							else b->bookmark[m].pos -= n;
				}
			}

			if (!b->redoing) {
				if (b->undoing) add_to_stream(&b->undo.redo, &ld->line[pos], n);
				else if (b->opt.do_undo) add_to_undo_stream(&b->undo, &ld->line[pos], n);
			}

			if (n == ld->line_len - pos) free_chars(b, &ld->line[pos], n);
			else {
				if (pos < ld->line_len / 2) {
					memmove(ld->line + n, ld->line, pos);
					free_chars(b, ld->line, n);
					ld->line += n;
				}
				else {
					memmove(ld->line + pos, ld->line + pos + n, ld->line_len - pos - n);
					free_chars(b, &ld->line[ld->line_len - n], n);
				}
			}

			if (!(ld->line_len -= n)) ld->line = NULL;
			len -= n;

			assert_line_desc(ld, b->encoding);
		}
		b->is_modified = 1;
	}

	release_signals();
	return OK;
}


/* Deletes a single character. */

int delete_one_char(buffer * const b, line_desc * const ld, const int line, const int pos) {
	return delete_stream(b, ld, line, pos, b->encoding == ENC_UTF8 && pos < ld->line_len ? utf8len(ld->line[pos]) : 1);
}



/* Changes the buffer file name to the given string, which must have been
	obtained through malloc(). */

void change_filename(buffer * const b, char * const name) {

	assert(name != NULL);

	if (b->filename) free(b->filename);
	b->filename = name;
}


/* Here we load a file into a given buffer. The buffer lists are deallocated
	first. If there is not write access to the file, the read-only flag is set.
	Note that we consider line feeds 0x0A's, 0x0D's and 0x00's (the last being
	made necessary by the way the pools are handled), unless the binary flag is
	set, in which case we consider only the 0x00's. */

int load_file_in_buffer(buffer * const b, const char *name) {
	int fh, result;

	if (!b) return ERROR;

	assert_buffer(b);
	
	name = tilde_expand(name);

	if (is_migrated(name)) return FILE_IS_MIGRATED;
	if (is_directory(name)) return FILE_IS_DIRECTORY;
    
	if ((fh = open(name, READ_FLAGS)) >= 0) {
		result = load_fh_in_buffer(b, fh);
		close(fh);
		if (!result) b->opt.read_only = (access(name, W_OK) != 0);
		return result;
	}

	return CANT_OPEN_FILE;
}



/* This function, together with insert_stream and delete_stream, is the only
	way of modifying the contents of a buffer. While loading a file could have
	passed through insert_stream, it would have been intolerably slow for large
	files. The flexible pool struture of ne allows to load the file with a
	single read in a big pool. */

int load_fh_in_buffer(buffer *b, int fh) {
	

	int i, len, num_lines;
	encoding_type encoding;
	unsigned char *p, *q;
	char_pool *cp;
	line_desc_pool *ldp;
	unsigned char terminators[] = { 0x0d, 0x0a };

	if (b->opt.preserve_cr) terminators[0] = 0;
         
	len = lseek(fh, 0, SEEK_END);

	if (len < 0 || lseek(fh, 0, SEEK_SET)<0) return IO_ERROR;

	free_buffer_contents(b);

	if (!len) {
		clear_buffer(b);
		b->encoding = ENC_ASCII;
		if (b->opt.do_undo) b->undo.last_save_step = 0;
		return OK;
	}

	block_signals();

	if (cp = alloc_char_pool(len + STANDARD_INCREMENT)) {

		if (read(fh, cp->pool, len) < len) {
			free_char_pool(cp);
			release_signals();
			return IO_ERROR;
		}

		b->allocated_chars = cp->size;
		b->free_chars = cp->size - len;

		p = cp->pool;

		/* This is the first pass on the data we just read. We count the number
		of lines. If we meet a CR/LF sequence and we did not ask for binary
		files, we decide the file is of CR/LF type. Note that this cannot happen
		if preserve_cr is set. */

		for(num_lines = i=0; i < len; i++,p++)
			if (!b->opt.binary && (*p == terminators[0] || *p == terminators[1]) || !*p) {
				if (i < len - 1 && !b->opt.preserve_cr && p[0] == '\r' && p[1] == '\n') {
					b->is_CRLF = TRUE;
					p++, i++;
					b->free_chars++;
				}
				num_lines++;
				b->free_chars++;
			}

		num_lines++;

		/* Now, if UTF-8 auto-detection is enabled, we try to guess whether this
			buffer is in UTF-8. */
		
		encoding = detect_encoding(cp->pool, len);
		if (encoding == ENC_ASCII) b->encoding = ENC_ASCII;
		else {
			if (b->opt.utf8auto && encoding == ENC_UTF8) b->encoding = ENC_UTF8;
			else b->encoding = ENC_8_BIT;
		}

		if (ldp = alloc_line_desc_pool(num_lines + STANDARD_LINE_INCREMENT)) {

			p = cp->pool;

			/* This is the second pass. Here we find the actual lines, and set to
			NUL the line terminators if necessary, following the same rationale of
			the first pass (this is important, as b->free_chars has been computed
			on the first pass).*/

			for(i = 0; i < num_lines; i++) {
				line_desc *ld = do_syntax ? &ldp->pool[i] : (line_desc *)&((no_syntax_line_desc *)ldp->pool)[i];
				rem(&ld->ld_node);
				add_tail(&b->line_desc_list, &ld->ld_node);
				
				/* ultima riga */
				if (i == num_lines - 1) {
					if (p - cp->pool < len) {
						assert(*p && *p != terminators[0] && *p != terminators[1]);
						ld->line = p;
						ld->line_len = len - (p - cp->pool);
					}
				}
				
				else {
					q = p;
					while((b->opt.binary || *q != terminators[0] && *q != terminators[1]) && *q) q++;
					
					ld->line_len = q - p;
					ld->line = q - p ? p : NULL;
					
					if (q - cp->pool < len - 1 && !b->opt.preserve_cr && q[0] == '\r' && q[1] == '\n') *q++ = 0;
					*q++ = 0;
					p = q;
					
				}
			}


			ldp->allocated_items = num_lines;
			
			

			/* We set correctly the offsets of the first and last character used. If no
			character is used (i.e., we have a file of line feeds), the char pool is
			freed. */

			if (b->free_chars < b->allocated_chars) {
				cp->last_used = len;
				while(!cp->pool[cp->first_used]) cp->first_used++;
				while(!cp->pool[cp->last_used]) cp->last_used--;
				add_head(&b->char_pool_list, &cp->cp_node);

				assert_char_pool(cp);
			}
			else free_char_pool(cp);

			add_head(&b->line_desc_pool_list, &ldp->ldp_node);

			b->num_lines = num_lines;

			reset_position_to_sof(b);
			if (b->opt.do_undo) b->undo.last_save_step = 0;
			reset_syntax_states(b);
			release_signals();
			return OK;
		}
		free_char_pool(cp);
	}

	release_signals();
	return OUT_OF_MEMORY;
}

/* Recomputes initial states for all lines in a buffer. */

void reset_syntax_states(buffer *b) {
	if (b->syn) {
		line_desc *ld;
		HIGHLIGHT_STATE next_line_state = { 0, 0, "" };
		ld = (line_desc *)b->line_desc_list.head;
		while(ld->ld_node.next) {
			ld->highlight_state = next_line_state;
			next_line_state = parse(b->syn, ld, next_line_state, b->encoding == ENC_UTF8);
			ld = (line_desc *)ld->ld_node.next;
		}
	}	
}


/* Ensures that the attribute buffer of this buffer is large enough. */

void ensure_attr_buf(buffer * const b, const int capacity) {
	/* attr_buf already exists? */
	if (!b->attr_buf) {
		b->attr_size = capacity;
		b->attr_buf = malloc(b->attr_size * sizeof *b->attr_buf); 
	}
	else if (capacity > b->attr_size) {
		b->attr_size = capacity;
		b->attr_buf = realloc(b->attr_buf, b->attr_size * sizeof *b->attr_buf);
	}
}


/* Here we save a buffer to a given file. If no file is specified, the
buffer filename field is used. The is_modified flag is set to 0. */


int save_buffer_to_file(buffer *b, const char *name) {

	int fh, error = OK;
	char *p;

	line_desc *ld = (line_desc *)b->line_desc_list.head;

	if (!b) return ERROR;

	assert_buffer(b);

	if (name == NULL) name = b->filename;

	if (!name) return ERROR;
	
	name = tilde_expand(name);

	if (is_migrated(name)) return FILE_IS_MIGRATED;
	if (is_directory(name)) return FILE_IS_MIGRATED;
    
	block_signals();

	if ((fh = open(name, WRITE_FLAGS, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH)) >= 0) {

		/* If we can allocate SAVE_BLOCK_LEN bytes, we will use
		them as a buffer for our saves. */

		if (p = malloc(SAVE_BLOCK_LEN + 1)) {

			/* used keeps track of the number of bytes used in the buffer. l, len
			specify the pointer to the block of characters to save, and
			its length. In case of very long lines, or buffer border crossing,
			they could point in the middle of a line descriptor. */

			int used = 0, len;
			char *l;

			while(ld->ld_node.next) {
				l = ld->line;
				len = ld->line_len;

				while(len > 0) {
					if (SAVE_BLOCK_LEN - used > len) {
						memcpy(p + used, l, len);
						used += len;
						len = 0;
					}
					else {
						memcpy(p + used, l, SAVE_BLOCK_LEN - used);
						len -= SAVE_BLOCK_LEN - used;
						l += SAVE_BLOCK_LEN - used;

						used = 0;

						if (write(fh, p, SAVE_BLOCK_LEN) < SAVE_BLOCK_LEN) {
							error = IO_ERROR;
							break;
						}
					}
				}

				if (error) break;

				ld = (line_desc *)ld->ld_node.next;

				/* Note that the two previous blocks never leave used == SAVE_BLOCK_LEN.
				Thus, we can always assume there are two free bytes at p+used. */

				if (ld->ld_node.next) {
					if (b->opt.binary) p[used++] = 0;
					else {
						if (b->is_CRLF) p[used++] = '\r';
						p[used++] = '\n';
					}
				}

				if (used >= SAVE_BLOCK_LEN) {
					if (write(fh, p, used) < used) {
						error = IO_ERROR;
						break;
					}
					else used = 0;
				}
			}

			if (!error && used && write(fh, p, used) < used) error = IO_ERROR;

			free(p);
		}
		else {

			/* If the buffer is not available, just save line by line. */

			while(ld->ld_node.next) {

				if (ld->line) {
					if (write(fh, ld->line, ld->line_len) < ld->line_len) {
						error = IO_ERROR;
						break;
					}
				}

				ld = (line_desc *)ld->ld_node.next;

				if (ld->ld_node.next) {
					if (!b->opt.binary && b->is_CRLF && write(fh, "\r", 1) < 1) {
						error = IO_ERROR;
						break;
					}
					if (write(fh, b->opt.binary ? "\0" : "\n", 1) < 1) {
						error = IO_ERROR;
						break;
					}
				}
			}
		}

		if (close(fh)) error = IO_ERROR;
		if (error == OK) b->is_modified = 0;

	}
	else error = CANT_OPEN_FILE;

	release_signals();
	return error;
}


/* Autosaves a given buffer. If the buffer has a name, a '#' is prefixed to
	it. If the buffer has no name, a fake name is generated using the PID of ne
	and the pointer to the buffer structure. This ensures uniqueness. Autosave
	never writes on the original file, also because it can be called during an
	emergency exit caused by a signal. */


void auto_save(buffer *b) {

	char *p;

	if (b->is_modified) {
		if (b->filename) {
			if (p = malloc(strlen(file_part(b->filename)) + 2)) {
				strcpy(p, "#");
				strcat(p, file_part(b->filename));
			}
		}
		else {
			if (p = malloc(MAX_INT_LEN * 2)) sprintf(p, "%p.%x", b, getpid());
		}
		save_buffer_to_file(b, p);
		free(p);
	}
}