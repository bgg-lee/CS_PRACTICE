/*-------------------------------------------------------------------------
 *
 * parser.c
 *		Main entry point/driver for PostgreSQL grammar
 *
 * Note that the grammar is not allowed to perform any table access
 * (since we need to be able to do basic parsing even while inside an
 * aborted transaction).  Therefore, the data structures returned by
 * the grammar are "raw" parsetrees that still need to be analyzed by
 * analyze.c and related files.
 *
 *
 * Portions Copyright (c) 1996-2024, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * IDENTIFICATION
 *	  src/backend/parser/parser.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include "gramparse.h"
#include "mb/pg_wchar.h"
#include "parser/parser.h"
#include "parser/scansup.h"

static bool check_uescapechar(unsigned char escape);
static char *str_udeescape(const char *str, char escape,
						   int position, core_yyscan_t yyscanner);

#include "nodes/nodes.h"          /* 기본 노드 구조체 */
#include "nodes/pg_list.h"        /* PostgreSQL List 구조체 */
#include "nodes/parsenodes.h"     /* SQL 구문 트리 노드 정의 */
#include "utils/elog.h"           /* PostgreSQL 로깅 함수 */
#include "lib/stringinfo.h"       /* 문자열 처리 유틸리티 */
#include "nodes/value.h" //where 구문 value 추가용 -> 효과가 없음
char *
generate_text_description(SelectStmt *selectStmt)
{
    StringInfoData buf;
    initStringInfo(&buf);

    /* SELECT 절 처리 */
    appendStringInfoString(&buf, "This Query Retrieves ");
    // elog(INFO, "Buffer initialized with: %s", buf.data);

    if (selectStmt->targetList == NULL)
    {
        appendStringInfoString(&buf, "No Columns ");
    }
    else
    {
        ListCell *lc;
        bool first = true;

        foreach (lc, selectStmt->targetList)
        {
            ResTarget *resTarget = (ResTarget *) lfirst(lc);

            if (resTarget == NULL)
            {
                elog(WARNING, "NULL ResTarget in targetList.");
                continue;
            }

            /* 쉼표 처리 */
            if (!first)
                appendStringInfoString(&buf, ", ");
            first = false;

            /* 컬럼 이름 처리 */ 
            if (resTarget->name)
            {
                appendStringInfo(&buf, "column '%s'", resTarget->name);
            }
            else if (IsA(resTarget->val, ColumnRef))
            {
                ColumnRef *colRef = (ColumnRef *) resTarget->val;

                if (colRef->fields != NIL)
                {
                    Node *firstField = (Node *) linitial(colRef->fields);
                    if (IsA(firstField, A_Star)) // '*' 가 입력된 경우
                    {
                        appendStringInfoString(&buf, "All Records");
                    }
                    else // 수정 들어갑니다.. column 명이 입력된 경우
                    {
                        if (colRef->fields == NIL)
                        {
                            elog(WARNING, "ColumnRef fields are NIL.");
                            appendStringInfoString(&buf, "unknown column, ");
                        }
                        else
                        {
                            /* "Record of"는 targetList 루프의 첫 번째 컬럼에서만 추가 */
                            if (buf.len == strlen("This Query Retrieves "))
                            {
                                appendStringInfoString(&buf, "Records Of Field ");
                            }

                            /* fields 리스트 순회 */
                            ListCell *fieldCell;
                            foreach (fieldCell, colRef->fields)
                            {
                                Node *fieldNode = (Node *) lfirst(fieldCell);

                                if (IsA(fieldNode, String)) // 필드가 문자열인지 확인
                                {
                                    appendStringInfo(&buf, "%s", strVal(fieldNode));
                                }
                                else
                                {
                                    elog(WARNING, "Unexpected node type in ColumnRef fields: %d", nodeTag(fieldNode));
                                    appendStringInfo(&buf, "unknown_field(type:%d).", nodeTag(fieldNode));
                                }
                            }
                        }
                    }
                }
                else
                {
                    elog(WARNING, "ColumnRef fields are NIL.");
                    appendStringInfoString(&buf, "unknown column, ");
                }
            }
        }
    }

/* FROM 절 처리 */
if (selectStmt->fromClause == NULL)
{
    appendStringInfoString(&buf, " With No FROM Clause");
}
else
{
    appendStringInfoString(&buf, " From ");
    ListCell *lc;
    foreach (lc, selectStmt->fromClause)
    {
        if (IsA(lfirst(lc), RangeVar))
        {
            RangeVar *rangeVar = (RangeVar *) lfirst(lc);
            if (rangeVar->relname)
                appendStringInfo(&buf, "Data Table Set '%s', ", rangeVar->relname);
        }
    }

    /* 마지막 쉼표 안전하게 제거 */
    if (buf.len > 2 && buf.data[buf.len - 2] == ',')
    {
        /* 안전하게 문자열 길이를 줄임 */
        buf.len -= 2;
        buf.data[buf.len] = '\0';
    }
}

/* WHERE 절 처리 */
if (selectStmt->whereClause != NULL)
{
    // elog(INFO, "WHERE clause detected, nodeTag: %d", nodeTag(selectStmt->whereClause));
    appendStringInfoString(&buf, " With Condition ");

    if (IsA(selectStmt->whereClause, A_Expr))
    {
        A_Expr *expr = (A_Expr *) selectStmt->whereClause;

        /* 왼쪽 피연산자 처리 */
        // elog(INFO, "Processing left operand...");
        if (IsA(expr->lexpr, ColumnRef))
        {
            ColumnRef *colRef = (ColumnRef *) expr->lexpr;
            if (colRef->fields != NIL)
            {
                Node *field = (Node *) linitial(colRef->fields);
                if (IsA(field, String))
                {
                    const char *fieldName = strVal(field);
                    appendStringInfo(&buf, "Field '%s' ", fieldName);
                    // elog(INFO, "Left operand: Field '%s'", fieldName);
                }
                else
                {
                    // elog(WARNING, "Unsupported ColumnRef field type in lexpr: %d", nodeTag(field));
                    appendStringInfoString(&buf, "unknown field ");
                }
            }
            else
            {
                elog(WARNING, "ColumnRef fields are NIL in lexpr.");
                appendStringInfoString(&buf, "unknown field ");
            }
        }
        else
        {
            elog(WARNING, "Unsupported left operand type in WHERE clause: %d", nodeTag(expr->lexpr));
            appendStringInfoString(&buf, "unknown left operand ");
        }

        /* 연산자 처리 */
        // elog(INFO, "Processing operator...");
        if (list_length(expr->name) == 1)
        {
            Node *opNode = (Node *) linitial(expr->name);
            if (IsA(opNode, String))
            {
                const char *operatorStr = strVal(opNode);
                // elog(INFO, "Operator: %s", operatorStr);
                if (strcmp(operatorStr, "=") == 0)
                {
                    appendStringInfoString(&buf, "is equal to ");
                }
                else if (strcmp(operatorStr, ">") == 0)
                {
                    appendStringInfoString(&buf, "is greater than ");
                }
                else if (strcmp(operatorStr, "<") == 0)
                {
                    appendStringInfoString(&buf, "is less than ");
                }
                else
                {
                    elog(WARNING, "Unsupported operator in WHERE clause: %s", operatorStr);
                    appendStringInfoString(&buf, "unknown operator ");
                }
            }
            else
            {
                elog(WARNING, "Unsupported operator node type in WHERE clause, nodeTag: %d", nodeTag(opNode));
                appendStringInfoString(&buf, "unknown operator ");
            }
        }
        else
        {
            elog(WARNING, "Unsupported operator structure in WHERE clause.");
        }

        /* 오른쪽 피연산자 처리 */
        // elog(INFO, "Processing right operand...");
        if (IsA(expr->rexpr, A_Const))
        {
            A_Const *constVal = (A_Const *) expr->rexpr;

            if (IsA(&constVal->val, Integer))
            {
                int rightValue = intVal(&constVal->val);
                appendStringInfo(&buf, "%d", rightValue);
                // elog(INFO, "Right operand is Integer: %d", rightValue);
            }
            else if (IsA(&constVal->val, Float))
            {
                const char *rightValue = strVal(&constVal->val);
                appendStringInfo(&buf, "%s", rightValue);
                // elog(INFO, "Right operand is Float: %s", rightValue);
            }
            else if (IsA(&constVal->val, String))
            {
                const char *rightValue = strVal(&constVal->val);
                appendStringInfo(&buf, "'%s'", rightValue);
                // elog(INFO, "Right operand is String: '%s'", rightValue);
            }
            else
            {
                elog(WARNING, "Unsupported A_Const type in rexpr.");
                appendStringInfoString(&buf, "unknown value");
            }
        }
        else
        {
            elog(WARNING, "Unsupported right operand in WHERE clause, nodeTag: %d", nodeTag(expr->rexpr));
            appendStringInfoString(&buf, "unknown right operand ");
        }
    }
    else
    {
        elog(WARNING, "Unsupported WHERE clause structure, nodeTag: %d", nodeTag(selectStmt->whereClause));
        appendStringInfoString(&buf, "unknown condition");
    }
}
else
{
    // elog(INFO, "No WHERE clause found.");
}

// elog(INFO, "Final NL Description: %s", buf.data);
return buf.data;
}

/*
 * raw_parser
 *		Given a query in string form, do lexical and grammatical analysis.
 *
 * Returns a list of raw (un-analyzed) parse trees.  The contents of the
 * list have the form required by the specified RawParseMode.
 */
List *
raw_parser(const char *str, RawParseMode mode)
{
    core_yyscan_t yyscanner;
    base_yy_extra_type yyextra;
    int yyresult;

    // Initialize scanner
    yyscanner = scanner_init(str, &yyextra.core_yy_extra,
                             &ScanKeywords, ScanKeywordTokens);

    if (mode == RAW_PARSE_DEFAULT)
        yyextra.have_lookahead = false;
    else
    {
        static const int mode_token[] = {
            [RAW_PARSE_DEFAULT] = 0,
            [RAW_PARSE_TYPE_NAME] = MODE_TYPE_NAME,
            [RAW_PARSE_PLPGSQL_EXPR] = MODE_PLPGSQL_EXPR,
            [RAW_PARSE_PLPGSQL_ASSIGN1] = MODE_PLPGSQL_ASSIGN1,
            [RAW_PARSE_PLPGSQL_ASSIGN2] = MODE_PLPGSQL_ASSIGN2,
            [RAW_PARSE_PLPGSQL_ASSIGN3] = MODE_PLPGSQL_ASSIGN3,
        };

        yyextra.have_lookahead = true;
        yyextra.lookahead_token = mode_token[mode];
        yyextra.lookahead_yylloc = 0;
        yyextra.lookahead_end = NULL;
    }

    // Initialize parser
    parser_init(&yyextra);

    // Parse query
    yyresult = base_yyparse(yyscanner);

    // Clean up scanner
    scanner_finish(yyscanner);

    if (yyresult)
        return NIL;

    if (nodeTag(yyextra.parsetree) != T_List)
    {
        elog(ERROR, "Parsed tree is not a T_List. Node details: %s", nodeToString(yyextra.parsetree));
        return NIL;
    }

    List *parsetree_list = (List *) yyextra.parsetree;

    // Process nodes in the parse tree list
ListCell *lc;
    foreach(lc, parsetree_list)
    {
        Node *node = (Node *) lfirst(lc);

        // elog(INFO, "Node tag: %d", nodeTag(node));

        if (node == NULL)
        {
            elog(ERROR, "NULL node found in parse tree list.");
            continue;
        }

        if (nodeTag(node) != T_RawStmt)
        {
            elog(WARNING, "Unexpected node type in parse tree list: %d", nodeTag(node));
            continue;
        }

        RawStmt *rawStmt = (RawStmt *) node;

        if (rawStmt->stmt == NULL)
        {
            elog(ERROR, "RawStmt->stmt is NULL.");
            continue;
        }

        // elog(INFO, "RawStmt->stmt type: %d", nodeTag(rawStmt->stmt));

        if (nodeTag(rawStmt->stmt) != T_SelectStmt)
        {
            elog(WARNING, "Skipping unsupported statement type: %d", nodeTag(rawStmt->stmt));
            continue;
        }

        SelectStmt *selectStmt = (SelectStmt *) rawStmt->stmt;

        if (selectStmt->targetList == NIL)
        {
            elog(WARNING, "SelectStmt->targetList is NIL. Skipping query.");
            continue;
        }

        // elog(INFO, "SelectStmt->targetList has %d items.", list_length(selectStmt->targetList));

        char *description = generate_text_description(selectStmt);
        elog(INFO, "SQL TO NL : %s", description);
    }

    return parsetree_list;
}


/*
 * Intermediate filter between parser and core lexer (core_yylex in scan.l).
 *
 * This filter is needed because in some cases the standard SQL grammar
 * requires more than one token lookahead.  We reduce these cases to one-token
 * lookahead by replacing tokens here, in order to keep the grammar LALR(1).
 *
 * Using a filter is simpler than trying to recognize multiword tokens
 * directly in scan.l, because we'd have to allow for comments between the
 * words.  Furthermore it's not clear how to do that without re-introducing
 * scanner backtrack, which would cost more performance than this filter
 * layer does.
 *
 * We also use this filter to convert UIDENT and USCONST sequences into
 * plain IDENT and SCONST tokens.  While that could be handled by additional
 * productions in the main grammar, it's more efficient to do it like this.
 *
 * The filter also provides a convenient place to translate between
 * the core_YYSTYPE and YYSTYPE representations (which are really the
 * same thing anyway, but notationally they're different).
 */
int
base_yylex(YYSTYPE *lvalp, YYLTYPE *llocp, core_yyscan_t yyscanner)
{
	base_yy_extra_type *yyextra = pg_yyget_extra(yyscanner);
	int			cur_token;
	int			next_token;
	int			cur_token_length;
	YYLTYPE		cur_yylloc;

	/* Get next token --- we might already have it */
	if (yyextra->have_lookahead)
	{
		cur_token = yyextra->lookahead_token;
		lvalp->core_yystype = yyextra->lookahead_yylval;
		*llocp = yyextra->lookahead_yylloc;
		if (yyextra->lookahead_end)
			*(yyextra->lookahead_end) = yyextra->lookahead_hold_char;
		yyextra->have_lookahead = false;
	}
	else
		cur_token = core_yylex(&(lvalp->core_yystype), llocp, yyscanner);

	/*
	 * If this token isn't one that requires lookahead, just return it.  If it
	 * does, determine the token length.  (We could get that via strlen(), but
	 * since we have such a small set of possibilities, hardwiring seems
	 * feasible and more efficient --- at least for the fixed-length cases.)
	 */
	switch (cur_token)
	{
		case FORMAT:
			cur_token_length = 6;
			break;
		case NOT:
			cur_token_length = 3;
			break;
		case NULLS_P:
			cur_token_length = 5;
			break;
		case WITH:
			cur_token_length = 4;
			break;
		case UIDENT:
		case USCONST:
			cur_token_length = strlen(yyextra->core_yy_extra.scanbuf + *llocp);
			break;
		case WITHOUT:
			cur_token_length = 7;
			break;
		default:
			return cur_token;
	}

	/*
	 * Identify end+1 of current token.  core_yylex() has temporarily stored a
	 * '\0' here, and will undo that when we call it again.  We need to redo
	 * it to fully revert the lookahead call for error reporting purposes.
	 */
	yyextra->lookahead_end = yyextra->core_yy_extra.scanbuf +
		*llocp + cur_token_length;
	Assert(*(yyextra->lookahead_end) == '\0');

	/*
	 * Save and restore *llocp around the call.  It might look like we could
	 * avoid this by just passing &lookahead_yylloc to core_yylex(), but that
	 * does not work because flex actually holds onto the last-passed pointer
	 * internally, and will use that for error reporting.  We need any error
	 * reports to point to the current token, not the next one.
	 */
	cur_yylloc = *llocp;

	/* Get next token, saving outputs into lookahead variables */
	next_token = core_yylex(&(yyextra->lookahead_yylval), llocp, yyscanner);
	yyextra->lookahead_token = next_token;
	yyextra->lookahead_yylloc = *llocp;

	*llocp = cur_yylloc;

	/* Now revert the un-truncation of the current token */
	yyextra->lookahead_hold_char = *(yyextra->lookahead_end);
	*(yyextra->lookahead_end) = '\0';

	yyextra->have_lookahead = true;

	/* Replace cur_token if needed, based on lookahead */
	switch (cur_token)
	{
		case FORMAT:
			/* Replace FORMAT by FORMAT_LA if it's followed by JSON */
			switch (next_token)
			{
				case JSON:
					cur_token = FORMAT_LA;
					break;
			}
			break;

		case NOT:
			/* Replace NOT by NOT_LA if it's followed by BETWEEN, IN, etc */
			switch (next_token)
			{
				case BETWEEN:
				case IN_P:
				case LIKE:
				case ILIKE:
				case SIMILAR:
					cur_token = NOT_LA;
					break;
			}
			break;

		case NULLS_P:
			/* Replace NULLS_P by NULLS_LA if it's followed by FIRST or LAST */
			switch (next_token)
			{
				case FIRST_P:
				case LAST_P:
					cur_token = NULLS_LA;
					break;
			}
			break;

		case WITH:
			/* Replace WITH by WITH_LA if it's followed by TIME or ORDINALITY */
			switch (next_token)
			{
				case TIME:
				case ORDINALITY:
					cur_token = WITH_LA;
					break;
			}
			break;

		case WITHOUT:
			/* Replace WITHOUT by WITHOUT_LA if it's followed by TIME */
			switch (next_token)
			{
				case TIME:
					cur_token = WITHOUT_LA;
					break;
			}
			break;

		case UIDENT:
		case USCONST:
			/* Look ahead for UESCAPE */
			if (next_token == UESCAPE)
			{
				/* Yup, so get third token, which had better be SCONST */
				const char *escstr;

				/* Again save and restore *llocp */
				cur_yylloc = *llocp;

				/* Un-truncate current token so errors point to third token */
				*(yyextra->lookahead_end) = yyextra->lookahead_hold_char;

				/* Get third token */
				next_token = core_yylex(&(yyextra->lookahead_yylval),
										llocp, yyscanner);

				/* If we throw error here, it will point to third token */
				if (next_token != SCONST)
					scanner_yyerror("UESCAPE must be followed by a simple string literal",
									yyscanner);

				escstr = yyextra->lookahead_yylval.str;
				if (strlen(escstr) != 1 || !check_uescapechar(escstr[0]))
					scanner_yyerror("invalid Unicode escape character",
									yyscanner);

				/* Now restore *llocp; errors will point to first token */
				*llocp = cur_yylloc;

				/* Apply Unicode conversion */
				lvalp->core_yystype.str =
					str_udeescape(lvalp->core_yystype.str,
								  escstr[0],
								  *llocp,
								  yyscanner);

				/*
				 * We don't need to revert the un-truncation of UESCAPE.  What
				 * we do want to do is clear have_lookahead, thereby consuming
				 * all three tokens.
				 */
				yyextra->have_lookahead = false;
			}
			else
			{
				/* No UESCAPE, so convert using default escape character */
				lvalp->core_yystype.str =
					str_udeescape(lvalp->core_yystype.str,
								  '\\',
								  *llocp,
								  yyscanner);
			}

			if (cur_token == UIDENT)
			{
				/* It's an identifier, so truncate as appropriate */
				truncate_identifier(lvalp->core_yystype.str,
									strlen(lvalp->core_yystype.str),
									true);
				cur_token = IDENT;
			}
			else if (cur_token == USCONST)
			{
				cur_token = SCONST;
			}
			break;
	}

	return cur_token;
}

/* convert hex digit (caller should have verified that) to value */
static unsigned int
hexval(unsigned char c)
{
	if (c >= '0' && c <= '9')
		return c - '0';
	if (c >= 'a' && c <= 'f')
		return c - 'a' + 0xA;
	if (c >= 'A' && c <= 'F')
		return c - 'A' + 0xA;
	elog(ERROR, "invalid hexadecimal digit");
	return 0;					/* not reached */
}

/* is Unicode code point acceptable? */
static void
check_unicode_value(pg_wchar c)
{
	if (!is_valid_unicode_codepoint(c))
		ereport(ERROR,
				(errcode(ERRCODE_SYNTAX_ERROR),
				 errmsg("invalid Unicode escape value")));
}

/* is 'escape' acceptable as Unicode escape character (UESCAPE syntax) ? */
static bool
check_uescapechar(unsigned char escape)
{
	if (isxdigit(escape)
		|| escape == '+'
		|| escape == '\''
		|| escape == '"'
		|| scanner_isspace(escape))
		return false;
	else
		return true;
}

/*
 * Process Unicode escapes in "str", producing a palloc'd plain string
 *
 * escape: the escape character to use
 * position: start position of U&'' or U&"" string token
 * yyscanner: context information needed for error reports
 */
static char *
str_udeescape(const char *str, char escape,
			  int position, core_yyscan_t yyscanner)
{
	const char *in;
	char	   *new,
			   *out;
	size_t		new_len;
	pg_wchar	pair_first = 0;
	ScannerCallbackState scbstate;

	/*
	 * Guesstimate that result will be no longer than input, but allow enough
	 * padding for Unicode conversion.
	 */
	new_len = strlen(str) + MAX_UNICODE_EQUIVALENT_STRING + 1;
	new = palloc(new_len);

	in = str;
	out = new;
	while (*in)
	{
		/* Enlarge string if needed */
		size_t		out_dist = out - new;

		if (out_dist > new_len - (MAX_UNICODE_EQUIVALENT_STRING + 1))
		{
			new_len *= 2;
			new = repalloc(new, new_len);
			out = new + out_dist;
		}

		if (in[0] == escape)
		{
			/*
			 * Any errors reported while processing this escape sequence will
			 * have an error cursor pointing at the escape.
			 */
			setup_scanner_errposition_callback(&scbstate, yyscanner,
											   in - str + position + 3);	/* 3 for U&" */
			if (in[1] == escape)
			{
				if (pair_first)
					goto invalid_pair;
				*out++ = escape;
				in += 2;
			}
			else if (isxdigit((unsigned char) in[1]) &&
					 isxdigit((unsigned char) in[2]) &&
					 isxdigit((unsigned char) in[3]) &&
					 isxdigit((unsigned char) in[4]))
			{
				pg_wchar	unicode;

				unicode = (hexval(in[1]) << 12) +
					(hexval(in[2]) << 8) +
					(hexval(in[3]) << 4) +
					hexval(in[4]);
				check_unicode_value(unicode);
				if (pair_first)
				{
					if (is_utf16_surrogate_second(unicode))
					{
						unicode = surrogate_pair_to_codepoint(pair_first, unicode);
						pair_first = 0;
					}
					else
						goto invalid_pair;
				}
				else if (is_utf16_surrogate_second(unicode))
					goto invalid_pair;

				if (is_utf16_surrogate_first(unicode))
					pair_first = unicode;
				else
				{
					pg_unicode_to_server(unicode, (unsigned char *) out);
					out += strlen(out);
				}
				in += 5;
			}
			else if (in[1] == '+' &&
					 isxdigit((unsigned char) in[2]) &&
					 isxdigit((unsigned char) in[3]) &&
					 isxdigit((unsigned char) in[4]) &&
					 isxdigit((unsigned char) in[5]) &&
					 isxdigit((unsigned char) in[6]) &&
					 isxdigit((unsigned char) in[7]))
			{
				pg_wchar	unicode;

				unicode = (hexval(in[2]) << 20) +
					(hexval(in[3]) << 16) +
					(hexval(in[4]) << 12) +
					(hexval(in[5]) << 8) +
					(hexval(in[6]) << 4) +
					hexval(in[7]);
				check_unicode_value(unicode);
				if (pair_first)
				{
					if (is_utf16_surrogate_second(unicode))
					{
						unicode = surrogate_pair_to_codepoint(pair_first, unicode);
						pair_first = 0;
					}
					else
						goto invalid_pair;
				}
				else if (is_utf16_surrogate_second(unicode))
					goto invalid_pair;

				if (is_utf16_surrogate_first(unicode))
					pair_first = unicode;
				else
				{
					pg_unicode_to_server(unicode, (unsigned char *) out);
					out += strlen(out);
				}
				in += 8;
			}
			else
				ereport(ERROR,
						(errcode(ERRCODE_SYNTAX_ERROR),
						 errmsg("invalid Unicode escape"),
						 errhint("Unicode escapes must be \\XXXX or \\+XXXXXX.")));

			cancel_scanner_errposition_callback(&scbstate);
		}
		else
		{
			if (pair_first)
				goto invalid_pair;

			*out++ = *in++;
		}
	}

	/* unfinished surrogate pair? */
	if (pair_first)
		goto invalid_pair;

	*out = '\0';
	return new;

	/*
	 * We might get here with the error callback active, or not.  Call
	 * scanner_errposition to make sure an error cursor appears; if the
	 * callback is active, this is duplicative but harmless.
	 */
invalid_pair:
	ereport(ERROR,
			(errcode(ERRCODE_SYNTAX_ERROR),
			 errmsg("invalid Unicode surrogate pair"),
			 scanner_errposition(in - str + position + 3,	/* 3 for U&" */
								 yyscanner)));
	return NULL;				/* keep compiler quiet */
}
