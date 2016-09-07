#include "lexer.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include "../util/log.h"
#include "../util/mem_utils.h"

static char* _current_file_name;
static FILE* _current_file;
static char _lookahead;
FIFO* _tokens;

TokenPos _current_pos = { 1, 1 };
TokenPos _current_tok_start = { 1, 1 };

void destroy_token(void* tok)
{
	Token* token = (Token*) tok;

	if (token) {
		if (token->type) {
			switch (token->type) {
				case TOK_EOF:
					break;

				case TOK_IDENT:
					destroy(token->val->string);
					break;

				case TOK_INTEGER_LITERAL:
					destroy(token->val->integer);
					break;

				default:
					log_err("type not supported\n");
					break;
			}
		}

		if (token->val)
			destroy(token->val);

		destroy(token);
	}
}

void init_lexer(char* fname)
{
	_current_file_name = fname;
	_current_file = fopen(fname, "r");
	_tokens = init_fifo_objects(destroy_token);
}

void destroy_lexer()
{
	if (_tokens)
		destroy_fifo(_tokens);

	if (_current_file)
		fclose(_current_file);
}

static void lexer_error(const char* fmt, ...)
{
	fprintf(stderr, "%s[ERROR]%s ", ANSI_COLOR_RED, ANSI_COLOR_RESET);
	char msg[250];

	sprintf(msg, "[%s:%zu:%zu] ", _current_file_name, _current_pos.line, _current_pos.column);
	strcat(msg, fmt);

	va_list args;

	va_start(args, fmt);
	vprintf(msg, args);
	va_end(args);
}

static void next_token()
{
	_lookahead = fgetc(_current_file);
}

static void adjust_position()
{
	if (_lookahead == '\n') {
		++_current_pos.line;
		_current_pos.column = 1;
	} else if (_lookahead == '\t') {
		_current_pos.column += 4;
	} else if (isprint(_lookahead)) {
		_current_pos.column += 1;
	}
}

static bool is_separator()
{
	switch (_lookahead) {
		case '(':
		case ')':
		case ',':
		case '.':
		case ' ':
		case ';':
		case '\t':
		case '\n':
		case EOF:
			return true;
	}

	return false;
}

static char* capture_string()
{
	size_t size;
	char* string = NULL;

	fseek(_current_file, -1, SEEK_CUR);
	unsigned long start = ftell(_current_file);

	for (size = 0; !is_separator(); size++)
		next_token();

	fseek(_current_file, start, SEEK_SET);
	string = malloc(size + 1);

	if (fgets(string, size, _current_file) == 0)
		log_kill("failed to retrieve string\n");

	string[size] = '\0';

	return string;
}

static inline void save_token_start()
{
	_current_tok_start.line = _current_pos.line;
	_current_tok_start.column = _current_pos.column;
}

static TokenValue* init_token_value(TokenType type)
{
	TokenValue* val = calloc(1, sizeof(TokenValue));

	switch (type) {
		case TOK_EOF:
		case TOK_IDENT:
		case TOK_INTEGER_LITERAL:
			break;

		default:
			log_err("unknown token type\n");
			break;
	}

	return val;
}

static Token* create_token(TokenType type, void* val)
{
	Token* token = malloc(sizeof(Token));
	token->val = init_token_value(type);
	token->type = type;
	token->pos.line = _current_tok_start.line;
	token->pos.column = _current_tok_start.column;

	switch (type) {
		case TOK_EOF:
			break;

		case TOK_IDENT:
			token->val->string = val;
			break;

		case TOK_INTEGER_LITERAL:
			token->val->integer = val;
			break;

		default:
			break;
	}

	return token;
}


static void tokenize()
{
	next_token();
	save_token_start();

	switch (_lookahead) {
		case ' ':
		case '\t':
		case '\n':
			return;

			/* Token symbols here */
		default: {
			switch (_lookahead) {
				case EOF:
					fifo_push(_tokens, create_token(TOK_EOF, NULL));
					return;
			}

			if (isalpha(_lookahead) || _lookahead == '_') {
				char* tmp = capture_string();
				fifo_push(_tokens, create_token(TOK_IDENT, tmp));
			} else if (isdigit(_lookahead)) {
				char* tmp = capture_string();
				uint64_t* value = malloc(sizeof(uint64_t));
				*value = (uint64_t) strtoll(tmp, 0, 0);
				destroy(tmp);
				fifo_push(_tokens, create_token(TOK_INTEGER_LITERAL, value));
			} else {
				lexer_error("unknown grammar\n");
			}

			break;
		}
	}

	adjust_position();

	/* IF-ELSE chain for types/keywords here */
}

void lex()
{
	do {
		tokenize();
	} while (!feof(_current_file));
}
