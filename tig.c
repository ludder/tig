 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
static int read_properties(FILE *pipe, const char *separators, int (*read)(char *, int, char *, int));
static void load_help_page(void);
#define TIG_LS_REMOTE \
	"git ls-remote . 2>/dev/null"

#define TIG_DIFF_CMD \
	"git show --patch-with-stat --find-copies-harder -B -C %s"

#define TIG_LOG_CMD	\
	"git log --cc --stat -n100 %s"

#define TIG_MAIN_CMD \
	"git log --topo-order --stat --pretty=raw %s"

/* XXX: Needs to be defined to the empty string. */
#define TIG_HELP_CMD	""
#define TIG_PAGER_CMD	""

struct int_map {
	const char *name;
	int namelen;
	int value;
};

static int
set_from_int_map(struct int_map *map, size_t map_size,
		 int *value, const char *name, int namelen)
{

	int i;

	for (i = 0; i < map_size; i++)
		if (namelen == map[i].namelen &&
		    !strncasecmp(name, map[i].name, namelen)) {
			*value = map[i].value;
			return OK;
		}

	return ERR;
}

static char *
chomp_string(char *name)
{
	int namelen;

	while (isspace(*name))
		name++;

	namelen = strlen(name) - 1;
	while (namelen > 0 && isspace(name[namelen]))
		name[namelen--] = 0;

	return name;
}

static bool
string_nformat(char *buf, size_t bufsize, int *bufpos, const char *fmt, ...)
{
	va_list args;
	int pos = bufpos ? *bufpos : 0;

	va_start(args, fmt);
	pos += vsnprintf(buf + pos, bufsize - pos, fmt, args);
	va_end(args);

	if (bufpos)
		*bufpos = pos;

	return pos >= bufsize ? FALSE : TRUE;
}

#define string_format(buf, fmt, args...) \
	string_nformat(buf, sizeof(buf), NULL, fmt, args)

#define string_format_from(buf, from, fmt, args...) \
	string_nformat(buf, sizeof(buf), from, fmt, args)
/*
 * User requests
 */

#define REQ_INFO \
	/* XXX: Keep the view request first and in sync with views[]. */ \
	REQ_GROUP("View switching") \
	REQ_(VIEW_MAIN,		"Show main view"), \
	REQ_(VIEW_DIFF,		"Show diff view"), \
	REQ_(VIEW_LOG,		"Show log view"), \
	REQ_(VIEW_HELP,		"Show help page"), \
	REQ_(VIEW_PAGER,	"Show pager view"), \
	\
	REQ_GROUP("View manipulation") \
	REQ_(ENTER,		"Enter current line and scroll"), \
	REQ_(NEXT,		"Move to next"), \
	REQ_(PREVIOUS,		"Move to previous"), \
	REQ_(VIEW_NEXT,		"Move focus to next view"), \
	REQ_(VIEW_CLOSE,	"Close the current view"), \
	REQ_(QUIT,		"Close all views and quit"), \
	\
	REQ_GROUP("Cursor navigation") \
	REQ_(MOVE_UP,		"Move cursor one line up"), \
	REQ_(MOVE_DOWN,		"Move cursor one line down"), \
	REQ_(MOVE_PAGE_DOWN,	"Move cursor one page down"), \
	REQ_(MOVE_PAGE_UP,	"Move cursor one page up"), \
	REQ_(MOVE_FIRST_LINE,	"Move cursor to first line"), \
	REQ_(MOVE_LAST_LINE,	"Move cursor to last line"), \
	\
	REQ_GROUP("Scrolling") \
	REQ_(SCROLL_LINE_UP,	"Scroll one line up"), \
	REQ_(SCROLL_LINE_DOWN,	"Scroll one line down"), \
	REQ_(SCROLL_PAGE_UP,	"Scroll one page up"), \
	REQ_(SCROLL_PAGE_DOWN,	"Scroll one page down"), \
	\
	REQ_GROUP("Misc") \
	REQ_(PROMPT,		"Bring up the prompt"), \
	REQ_(SCREEN_UPDATE,	"Update the screen"), \
	REQ_(SCREEN_REDRAW,	"Redraw the screen"), \
	REQ_(SCREEN_RESIZE,	"Resize the screen"), \
	REQ_(SHOW_VERSION,	"Show version information"), \
	REQ_(STOP_LOADING,	"Stop all loading views"), \
	REQ_(TOGGLE_LINENO,	"Toggle line numbers"),


/* User action requests. */
enum request {
#define REQ_GROUP(help)
#define REQ_(req, help) REQ_##req

	/* Offset all requests to avoid conflicts with ncurses getch values. */
	REQ_OFFSET = KEY_MAX + 1,
	REQ_INFO

#undef	REQ_GROUP
#undef	REQ_
};

struct request_info {
	enum request request;
	char *help;
};

static struct request_info req_info[] = {
#define REQ_GROUP(help)	{ 0, (help) },
#define REQ_(req, help)	{ REQ_##req, (help) }
	REQ_INFO
#undef	REQ_GROUP
#undef	REQ_
};

/*
 * Options
 */
"  -b[N], --tab-size[=N]       Set number of spaces for tab expansion\n"
static char opt_encoding[20]	= "";
static bool opt_utf8		= TRUE;
enum option_type {
	OPT_NONE,
	OPT_INT,
};

static bool
check_option(char *opt, char short_name, char *name, enum option_type type, ...)
{
	va_list args;
	char *value = "";
	int *number;

	if (opt[0] != '-')
		return FALSE;

	if (opt[1] == '-') {
		int namelen = strlen(name);

		opt += 2;

		if (strncmp(opt, name, namelen))
			return FALSE;

		if (opt[namelen] == '=')
			value = opt + namelen + 1;

	} else {
		if (!short_name || opt[1] != short_name)
			return FALSE;
		value = opt + 2;
	}

	va_start(args, type);
	if (type == OPT_INT) {
		number = va_arg(args, int *);
		if (isdigit(*value))
			*number = atoi(value);
	}
	va_end(args);

	return TRUE;
}

		if (check_option(opt, 'n', "line-number", OPT_INT, &opt_num_interval)) {
		if (check_option(opt, 'b', "tab-size", OPT_INT, &opt_tab_size)) {
			opt_tab_size = MIN(opt_tab_size, TABSIZE);
		if (check_option(opt, 'v', "version", OPT_NONE)) {
		if (check_option(opt, 'h', "help", OPT_NONE)) {
	if (*opt_encoding && strcasecmp(opt_encoding, "UTF-8"))
		opt_utf8 = FALSE;

static struct int_map color_map[] = {
#define COLOR_MAP(name) { #name, STRING_SIZE(#name), COLOR_##name }
	COLOR_MAP(DEFAULT),
	COLOR_MAP(BLACK),
	COLOR_MAP(BLUE),
	COLOR_MAP(CYAN),
	COLOR_MAP(GREEN),
	COLOR_MAP(MAGENTA),
	COLOR_MAP(RED),
	COLOR_MAP(WHITE),
	COLOR_MAP(YELLOW),
};

static struct int_map attr_map[] = {
#define ATTR_MAP(name) { #name, STRING_SIZE(#name), A_##name }
	ATTR_MAP(NORMAL),
	ATTR_MAP(BLINK),
	ATTR_MAP(BOLD),
	ATTR_MAP(DIM),
	ATTR_MAP(REVERSE),
	ATTR_MAP(STANDOUT),
	ATTR_MAP(UNDERLINE),
};
LINE(DIFF_HEADER,  "diff --git ",	COLOR_YELLOW,	COLOR_DEFAULT,	0), \
LINE(DIFF_INDEX,	"index ",	  COLOR_BLUE,	COLOR_DEFAULT,	0), \
LINE(DIFF_OLDMODE,	"old file mode ", COLOR_YELLOW,	COLOR_DEFAULT,	0), \
LINE(DIFF_NEWMODE,	"new file mode ", COLOR_YELLOW,	COLOR_DEFAULT,	0), \
LINE(DIFF_COPY_FROM,	"copy from",	  COLOR_YELLOW,	COLOR_DEFAULT,	0), \
LINE(DIFF_COPY_TO,	"copy to",	  COLOR_YELLOW,	COLOR_DEFAULT,	0), \
LINE(DIFF_RENAME_FROM,	"rename from",	  COLOR_YELLOW,	COLOR_DEFAULT,	0), \
LINE(DIFF_RENAME_TO,	"rename to",	  COLOR_YELLOW,	COLOR_DEFAULT,	0), \
LINE(DIFF_SIMILARITY,   "similarity ",	  COLOR_YELLOW,	COLOR_DEFAULT,	0), \
LINE(DIFF_DISSIMILARITY,"dissimilarity ", COLOR_YELLOW,	COLOR_DEFAULT,	0), \
LINE(DIFF_TREE,		"diff-tree ",	  COLOR_BLUE,	COLOR_DEFAULT,	0), \
LINE(PP_REFS,	   "Refs: ",		COLOR_RED,	COLOR_DEFAULT,	0), \
LINE(MAIN_REF,     "",			COLOR_CYAN,	COLOR_DEFAULT,	A_BOLD), \


/*
 * Line-oriented content detection.
 */
	const char *name;	/* Option name. */
	int namelen;		/* Size of option name. */
	{ #type, STRING_SIZE(#type), (line), STRING_SIZE(line), (fg), (bg), (attr) }
static struct line_info *
get_line_info(char *name, int namelen)
{
	enum line_type type;
	int i;

	/* Diff-Header -> DIFF_HEADER */
	for (i = 0; i < namelen; i++) {
		if (name[i] == '-')
			name[i] = '_';
		else if (name[i] == '.')
			name[i] = '_';
	}

	for (type = 0; type < ARRAY_SIZE(line_info); type++)
		if (namelen == line_info[type].namelen &&
		    !strncasecmp(line_info[type].name, name, namelen))
			return &line_info[type];

	return NULL;
}

struct line {
	enum line_type type;
	void *data;		/* User data */
};
/*
 * User config file handling.
 */
#define set_color(color, name, namelen) \
	set_from_int_map(color_map, ARRAY_SIZE(color_map), color, name, namelen)
#define set_attribute(attr, name, namelen) \
	set_from_int_map(attr_map, ARRAY_SIZE(attr_map), attr, name, namelen)
static int   config_lineno;
static bool  config_errors;
static char *config_msg;
static int
set_option(char *opt, int optlen, char *value, int valuelen)
{
	/* Reads: "color" object fgcolor bgcolor [attr] */
	if (!strcmp(opt, "color")) {
		struct line_info *info;

		value = chomp_string(value);
		valuelen = strcspn(value, " \t");
		info = get_line_info(value, valuelen);
		if (!info) {
			config_msg = "Unknown color name";
			return ERR;
		}

		value = chomp_string(value + valuelen);
		valuelen = strcspn(value, " \t");
		if (set_color(&info->fg, value, valuelen) == ERR) {
			config_msg = "Unknown color";
			return ERR;
		}

		value = chomp_string(value + valuelen);
		valuelen = strcspn(value, " \t");
		if (set_color(&info->bg, value, valuelen) == ERR) {
			config_msg = "Unknown color";
			return ERR;
		}

		value = chomp_string(value + valuelen);
		if (*value &&
		    set_attribute(&info->attr, value, strlen(value)) == ERR) {
			config_msg = "Unknown attribute";
			return ERR;
		}

		return OK;
	}

	return ERR;
}

static int
read_option(char *opt, int optlen, char *value, int valuelen)
{
	config_lineno++;
	config_msg = "Internal error";

	optlen = strcspn(opt, "#;");
	if (optlen == 0) {
		/* The whole line is a commend or empty. */
		return OK;

	} else if (opt[optlen] != 0) {
		/* Part of the option name is a comment, so the value part
		 * should be ignored. */
		valuelen = 0;
		opt[optlen] = value[valuelen] = 0;
	} else {
		/* Else look for comment endings in the value. */
		valuelen = strcspn(value, "#;");
		value[valuelen] = 0;
	}

	if (set_option(opt, optlen, value, valuelen) == ERR) {
		fprintf(stderr, "Error on line %d, near '%.*s' option: %s\n",
			config_lineno, optlen, opt, config_msg);
		config_errors = TRUE;
	}

	/* Always keep going if errors are encountered. */
	return OK;
}

static int
load_options(void)
{
	char *home = getenv("HOME");
	char buf[1024];
	FILE *file;
	config_lineno = 0;
	config_errors = FALSE;
	if (!home || !string_format(buf, "%s/.tigrc", home))
		return ERR;
	/* It's ok that the file doesn't exist. */
	file = fopen(buf, "r");
	if (!file)
		return OK;
	if (read_properties(file, " \t", read_option) == ERR ||
	    config_errors == TRUE)
		fprintf(stderr, "Errors while loading %s.\n", buf);
	return OK;
}


/*
 */
struct view_ops;
/* Current head and commit ID */
	struct view_ops *ops;	/* View operations */
	struct line *line;	/* Line index */
	unsigned long line_size;/* Total number of allocated lines */
struct view_ops {
	/* What type of content being displayed. Used in the title bar. */
	const char *type;
	/* Draw one line; @lineno must be < view->height. */
	bool (*draw)(struct view *view, struct line *line, unsigned int lineno);
	/* Read one line; updates view->line. */
	bool (*read)(struct view *view, char *data);
	/* Depending on view, change display based on current line. */
	bool (*enter)(struct view *view, struct line *line);
};

static bool
draw_view_line(struct view *view, unsigned int lineno)
{
	if (view->offset + lineno >= view->lines)
		return FALSE;

	return view->ops->draw(view, &view->line[view->offset + lineno], lineno);
}

		if (!draw_view_line(view, lineno))
	if (view->lines || view->pipe) {
		unsigned int lines = view->lines
				   ? (view->lineno + 1) * 100 / view->lines
				   : 0;

			lines);
	}

	if (view->pipe) {
		time_t secs = time(NULL) - view->start_time;

		/* Three git seconds are a long time ... */
		if (secs > 2)
			wprintw(view->title, " %lds", secs);
	wmove(view->title, 0, view->width - 1);
			view->win = newwin(view->height, 0, offset, 0);
			wresize(view->win, view->height, view->width);
static void
update_display_cursor(void)
{
	struct view *view = display[current_view];

	/* Move the cursor to the right-most column of the cursor line.
	 *
	 * XXX: This could turn out to be a bit expensive, but it ensures that
	 * the cursor does not jump around. */
	if (view->lines) {
		wmove(view->win, view->lineno - view->offset, view->width - 1);
		wrefresh(view->win);
	}
}
			if (!draw_view_line(view, line))
		draw_view_line(view, 0);
		draw_view_line(view, view->lineno - view->offset);
		draw_view_line(view,  prev_lineno);
	draw_view_line(view, view->lineno - view->offset);
		if (!string_format(view->cmd, format, id, id, id, id, id))
			if (view->line[i].data)
				free(view->line[i].data);
static struct line *
realloc_lines(struct view *view, size_t line_size)
{
	struct line *tmp = realloc(view->line, sizeof(*view->line) * line_size);

	if (!tmp)
		return NULL;

	view->line = tmp;
	view->line_size = line_size;
	return view->line;
}

	if (!realloc_lines(view, view->lines + lines))
		report("");
		display[1] = view;
			current_view = 1;
		if (split && !backgrounded) {
		}
	if (view == VIEW(REQ_VIEW_HELP))
		load_help_page();

	if (view->pipe && view->lines == 0) {
		report("");
		report("");
		return view->ops->enter(view, &view->line[view->lineno]);
	case REQ_TOGGLE_LINENO:
		for (i = 0; i < ARRAY_SIZE(views); i++) {
			view = &views[i];
				report("Stopped loading the %s view", view->name),
		/* XXX: Mark closed views by letting view->parent point to the
		 * view itself. Parents to closed view should never be
		 * followed. */
		if (view->parent &&
		    view->parent->parent != view->parent) {
			view->parent = view;
pager_draw(struct view *view, struct line *line, unsigned int lineno)
	char *text = line->data;
	enum line_type type = line->type;
	int textlen = strlen(text);
			string_copy(view->ref, text + 7);
		while (text && col_offset + col < view->width) {
			char *pos = text;
			if (*text == '\t') {
				text++;
				pos = spaces;
				text = strchr(text, '\t');
				cols = line ? text - pos : strlen(pos);
			waddnstr(view->win, pos, MIN(cols, cols_max));
		for (; pos < textlen && col < view->width; pos++, col++)
			if (text[pos] == '\t')
		waddnstr(view->win, text, pos);
static void
add_pager_refs(struct view *view, struct line *line)
{
	char buf[1024];
	char *data = line->data;
	struct ref **refs;
	int bufpos = 0, refpos = 0;
	const char *sep = "Refs: ";

	assert(line->type == LINE_COMMIT);

	refs = get_refs(data + STRING_SIZE("commit "));
	if (!refs)
		return;

	do {
		struct ref *ref = refs[refpos];
		char *fmt = ref->tag ? "%s[%s]" : "%s%s";

		if (!string_format_from(buf, &bufpos, fmt, sep, ref->name))
			return;
		sep = ", ";
	} while (refs[refpos++]->next);

	if (!realloc_lines(view, view->line_size + 1))
		return;

	line = &view->line[view->lines];
	line->data = strdup(buf);
	if (!line->data)
		return;

	line->type = LINE_PP_REFS;
	view->lines++;
}

pager_read(struct view *view, char *data)
	struct line *line = &view->line[view->lines];
	line->data = strdup(data);
	if (!line->data)
	line->type = get_line_type(line->data);

	if (line->type == LINE_COMMIT &&
	    (view == VIEW(REQ_VIEW_DIFF) ||
	     view == VIEW(REQ_VIEW_LOG)))
		add_pager_refs(view, line);

pager_enter(struct view *view, struct line *line)
	if (line->type == LINE_COMMIT &&
	   (view == VIEW(REQ_VIEW_LOG) ||
	    view == VIEW(REQ_VIEW_PAGER))) {
	 * but if we are scrolling a non-current view this won't properly
	 * update the view title. */
main_draw(struct view *view, struct line *line, unsigned int lineno)
	struct commit *commit = line->data;
	int trimmed = 1;
	if (opt_utf8) {
		authorlen = utf8_length(commit->author, AUTHOR_COLS - 2, &col, &trimmed);
	} else {
		authorlen = strlen(commit->author);
		if (authorlen > AUTHOR_COLS - 2) {
			authorlen = AUTHOR_COLS - 2;
			trimmed = 1;
		}
	}
	struct commit *commit = view->lines
			      ? view->line[view->lines - 1].data : NULL;
		view->line[view->lines++].data = commit;
		if (!commit)
			break;

		if (!commit)
main_enter(struct view *view, struct line *line)
/*
 * Keys
 */
	/* View switching */
	{ '?',		REQ_VIEW_HELP },
	/* View manipulation */
	/* Cursor navigation */
	/* Scrolling */
	/* Misc */
	{ 'n',		REQ_TOGGLE_LINENO },
struct key {
	char *name;
	int value;
};

static struct key key_table[] = {
	{ "Enter",	KEY_RETURN },
	{ "Space",	' ' },
	{ "Backspace",	KEY_BACKSPACE },
	{ "Tab",	KEY_TAB },
	{ "Escape",	KEY_ESC },
	{ "Left",	KEY_LEFT },
	{ "Right",	KEY_RIGHT },
	{ "Up",		KEY_UP },
	{ "Down",	KEY_DOWN },
	{ "Insert",	KEY_IC },
	{ "Delete",	KEY_DC },
	{ "Home",	KEY_HOME },
	{ "End",	KEY_END },
	{ "PageUp",	KEY_PPAGE },
	{ "PageDown",	KEY_NPAGE },
	{ "F1",		KEY_F(1) },
	{ "F2",		KEY_F(2) },
	{ "F3",		KEY_F(3) },
	{ "F4",		KEY_F(4) },
	{ "F5",		KEY_F(5) },
	{ "F6",		KEY_F(6) },
	{ "F7",		KEY_F(7) },
	{ "F8",		KEY_F(8) },
	{ "F9",		KEY_F(9) },
	{ "F10",	KEY_F(10) },
	{ "F11",	KEY_F(11) },
	{ "F12",	KEY_F(12) },
};

static char *
get_key(enum request request)
{
	static char buf[BUFSIZ];
	static char key_char[] = "'X'";
	int pos = 0;
	char *sep = "    ";
	int i;

	buf[pos] = 0;

	for (i = 0; i < ARRAY_SIZE(keymap); i++) {
		char *seq = NULL;
		int key;

		if (keymap[i].request != request)
			continue;

		for (key = 0; key < ARRAY_SIZE(key_table); key++)
			if (key_table[key].value == keymap[i].alias)
				seq = key_table[key].name;

		if (seq == NULL &&
		    keymap[i].alias < 127 &&
		    isprint(keymap[i].alias)) {
			key_char[1] = (char) keymap[i].alias;
			seq = key_char;
		}

		if (!seq)
			seq = "'?'";

		if (!string_format_from(buf, &pos, "%s%s", sep, seq))
			return "Too many keybindings!";
		sep = ", ";
	}

	return buf;
}

static void load_help_page(void)
{
	char buf[BUFSIZ];
	struct view *view = VIEW(REQ_VIEW_HELP);
	int lines = ARRAY_SIZE(req_info) + 2;
	int i;

	if (view->lines > 0)
		return;

	for (i = 0; i < ARRAY_SIZE(req_info); i++)
		if (!req_info[i].request)
			lines++;

	view->line = calloc(lines, sizeof(*view->line));
	if (!view->line) {
		report("Allocation failure");
		return;
	}

	pager_read(view, "Quick reference for tig keybindings:");

	for (i = 0; i < ARRAY_SIZE(req_info); i++) {
		char *key;

		if (!req_info[i].request) {
			pager_read(view, "");
			pager_read(view, req_info[i].help);
			continue;
		}

		key = get_key(req_info[i].request);
		if (!string_format(buf, "%-25s %s", key, req_info[i].help))
			continue;

		pager_read(view, buf);
	}
}

						/* CJK ... Yi */
	update_display_cursor();
static int
read_ref(char *id, int idlen, char *name, int namelen)
{
	struct ref *ref;
	bool tag = FALSE;
	bool tag_commit = FALSE;

	/* Commits referenced by tags has "^{}" appended. */
	if (name[namelen - 1] == '}') {
		while (namelen > 0 && name[namelen] != '^')
			namelen--;
		if (namelen > 0)
			tag_commit = TRUE;
		name[namelen] = 0;
	}

	if (!strncmp(name, "refs/tags/", STRING_SIZE("refs/tags/"))) {
		if (!tag_commit)
			return OK;
		name += STRING_SIZE("refs/tags/");
		tag = TRUE;

	} else if (!strncmp(name, "refs/heads/", STRING_SIZE("refs/heads/"))) {
		name += STRING_SIZE("refs/heads/");

	} else if (!strcmp(name, "HEAD")) {
		return OK;
	}

	refs = realloc(refs, sizeof(*refs) * (refs_size + 1));
	if (!refs)
		return ERR;

	ref = &refs[refs_size++];
	ref->name = strdup(name);
	if (!ref->name)
		return ERR;

	ref->tag = tag;
	string_copy(ref->id, id);

	return OK;
}

	return read_properties(popen(cmd, "r"), "\t", read_ref);
}
static int
read_repo_config_option(char *name, int namelen, char *value, int valuelen)
{
	if (!strcmp(name, "i18n.commitencoding")) {
		string_copy(opt_encoding, value);
	}
	return OK;
}
static int
load_repo_config(void)
{
	return read_properties(popen("git repo-config --list", "r"),
			       "=", read_repo_config_option);
}
static int
read_properties(FILE *pipe, const char *separators,
		int (*read_property)(char *, int, char *, int))
{
	char buffer[BUFSIZ];
	char *name;
	int state = OK;
	if (!pipe)
		return ERR;
	while (state == OK && (name = fgets(buffer, sizeof(buffer), pipe))) {
		char *value;
		size_t namelen;
		size_t valuelen;
		name = chomp_string(name);
		namelen = strcspn(name, separators);
		if (name[namelen]) {
			name[namelen] = 0;
			value = chomp_string(name + namelen + 1);
			valuelen = strlen(value);
		} else {
			value = "";
			valuelen = 0;
		}
		state = read_property(name, namelen, value, valuelen);
	if (state != ERR && ferror(pipe))
		state = ERR;
	return state;

	if (load_options() == ERR)
		die("Failed to load user config.");

	/* Load the repo config file so options can be overwritten from
	 * the command line.  */
	if (load_repo_config() == ERR)
		die("Failed to load repo config.");

				report("Prompt interrupted by loading view, "
				       "press 'z' to stop loading views");
				request = REQ_SCREEN_UPDATE;