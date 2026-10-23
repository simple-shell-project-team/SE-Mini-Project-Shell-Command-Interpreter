#include "../parser.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define WORD(value) {TOKEN_WORD, (char *)(value), NULL}
#define OPERATOR(value) {value, NULL, NULL}

static int failures = 0;
static int tests_run = 0;

static void report_failure(const char *name, const char *detail)
{
    fprintf(stderr, "FAIL: %s: %s\n", name, detail);
    ++failures;
}

static void expect_tokens(const char *name, const char *line,
                          const Token *expected, size_t expected_count)
{
    TokenList tokens;
    ParseError error;
    ParseStatus status;
    size_t index;

    ++tests_run;
    token_list_init(&tokens);
    status = parse_line(line, &tokens, &error);
    if (status != PARSE_STATUS_OK) {
        report_failure(name, error.message);
        token_list_free(&tokens);
        return;
    }
    if (tokens.count != expected_count) {
        report_failure(name, "unexpected token count");
        token_list_free(&tokens);
        return;
    }
    for (index = 0; index < expected_count; ++index) {
        if (tokens.items[index].type != expected[index].type) {
            report_failure(name, "unexpected token type");
            token_list_free(&tokens);
            return;
        }
        if (expected[index].type == TOKEN_WORD &&
            (tokens.items[index].text == NULL ||
             strcmp(tokens.items[index].text, expected[index].text) != 0)) {
            report_failure(name, "unexpected word text");
            token_list_free(&tokens);
            return;
        }
    }
    token_list_free(&tokens);
}

static void expect_error(const char *name, const char *line,
                         const char *expected_message)
{
    TokenList tokens;
    ParseError error;
    ParseStatus status;

    ++tests_run;
    token_list_init(&tokens);
    status = parse_line(line, &tokens, &error);
    if (status != PARSE_STATUS_SYNTAX_ERROR) {
        report_failure(name, "expected syntax error status 2");
    } else if (error.status != status) {
        report_failure(name, "error status does not match return value");
    } else if (strcmp(error.message, expected_message) != 0) {
        report_failure(name, error.message);
    } else if (tokens.count != 0) {
        report_failure(name, "failed parse returned partial tokens");
    }
    token_list_free(&tokens);
}

static void expect_expansion_mask(const char *name, const char *line,
                                  size_t token_index,
                                  const unsigned char *expected,
                                  size_t expected_length)
{
    TokenList tokens;
    ParseError error;
    ParseStatus status;

    ++tests_run;
    token_list_init(&tokens);
    status = parse_line(line, &tokens, &error);
    if (status != PARSE_STATUS_OK) {
        report_failure(name, error.message);
    } else if (token_index >= tokens.count ||
               tokens.items[token_index].type != TOKEN_WORD) {
        report_failure(name, "word token was not returned");
    } else if (strlen(tokens.items[token_index].text) != expected_length ||
               memcmp(tokens.items[token_index].expansion_mask, expected,
                      expected_length) != 0) {
        report_failure(name, "unexpected expansion context metadata");
    }
    token_list_free(&tokens);
}

static void expect_separate_validation(void)
{
    TokenList tokens;
    ParseError error;
    ParseStatus status;

    ++tests_run;
    token_list_init(&tokens);
    status = tokenize("echo hi >", &tokens, &error);
    if (status != PARSE_STATUS_OK) {
        report_failure("separate validation", "tokenizer rejected token sequence");
    } else if (tokens.count != 3) {
        report_failure("separate validation", "tokenizer returned wrong count");
    } else if (validate_syntax(&tokens, &error) !=
                   PARSE_STATUS_SYNTAX_ERROR ||
               strcmp(error.message, "missing file name") != 0) {
        report_failure("separate validation",
                       "validator missed the redirection error");
    } else if (error.status != PARSE_STATUS_SYNTAX_ERROR) {
        report_failure("separate validation", "validator status is inconsistent");
    }
    token_list_free(&tokens);
}

static void expect_large_input(void)
{
    const size_t argument_length = 100000;
    const size_t argument_count = 2000;
    size_t line_length = argument_length + argument_count * 2 - 2;
    char *line = (char *)malloc(line_length + 1);
    TokenList tokens;
    ParseError error;
    ParseStatus status;
    size_t offset;
    size_t index;

    ++tests_run;
    if (line == NULL) {
        report_failure("large input", "test allocation failed");
        return;
    }
    memset(line, 'x', argument_length);
    offset = argument_length;
    for (index = 0; index < argument_count - 1; ++index) {
        line[offset++] = ' ';
        line[offset++] = 'a';
    }
    if (offset != line_length) {
        report_failure("large input", "test input length calculation failed");
        free(line);
        return;
    }
    line[line_length] = '\0';

    token_list_init(&tokens);
    status = parse_line(line, &tokens, &error);
    if (status != PARSE_STATUS_OK || tokens.count != argument_count) {
        report_failure("large input", status == PARSE_STATUS_OK
                                          ? "unexpected token count"
                                          : error.message);
    } else if (strlen(tokens.items[0].text) != argument_length) {
        report_failure("large input", "long word was truncated");
    }
    token_list_free(&tokens);
    free(line);
}

static void run_tests(void)
{
    static const Token quoted_unsupported[] = {
        WORD("echo"), WORD("&& ||")
    };
    expect_tokens("quoted unsupported operators", "echo '&& ||'",
                  quoted_unsupported,
                  sizeof(quoted_unsupported) / sizeof(quoted_unsupported[0]));
}

int main(void)
{
    static const unsigned char expandable[] = {1U, 1U, 1U, 1U, 1U};
    static const unsigned char literal[] = {0U, 0U, 0U, 0U, 0U};
    static const unsigned char escaped_dollar[] = {0U, 1U, 1U, 1U, 1U};
    static const unsigned char mixed_quotes[] = {
        0U, 0U, 0U, 0U, 0U, 0U, 0U, 1U, 1U, 1U, 1U, 1U
    };
    static const Token normal[] = {
        WORD("echo"), WORD("hello"), WORD("world")
    };
    static const Token whitespace[] = {
        WORD("one"), WORD("two")
    };
    static const Token single_quotes[] = {
        WORD("echo"), WORD("a   b|c;d")
    };
    static const Token single_quote_backslash[] = {
        WORD("echo"), WORD("a\\ b")
    };
    static const Token double_quotes[] = {
        WORD("echo"), WORD("a b|c;d>e&f\"g")
    };
    static const Token escaped[] = {
        WORD("echo"), WORD("a b"), WORD(">"), WORD("c")
    };
    static const Token adjacent_operators[] = {
        WORD("echo"), WORD("x"), OPERATOR(TOKEN_REDIRECT_OUT), WORD("f"),
        OPERATOR(TOKEN_SEMICOLON), WORD("cat"),
        OPERATOR(TOKEN_REDIRECT_IN), WORD("f"),
        OPERATOR(TOKEN_REDIRECT_APPEND), WORD("g"),
        OPERATOR(TOKEN_AMPERSAND), WORD("echo"), WORD("done")
    };
    static const Token quoted_operators[] = {
        WORD("echo"), WORD("|<>>;&")
    };
    static const Token empty_arguments[] = {
        WORD("echo"), WORD(""), WORD(""), WORD("ab"), WORD("")
    };
    static const Token comments[] = {
        WORD("echo"), WORD("x#kept"), WORD("#also-kept")
    };
    static const Token separators_and_comments[] = {
        WORD("echo"), WORD("hi"), OPERATOR(TOKEN_SEMICOLON)
    };
    static const Token literal_expansion[] = {
        WORD("echo"), WORD("$HOME"), WORD("${name}")
    };
    static const Token escaped_hash[] = {
        WORD("echo"), WORD("#not-a-comment")
    };
    static const Token blank[] = { {TOKEN_WORD, NULL, NULL} };

    expect_tokens("normal words", "echo hello world", normal,
                  sizeof(normal) / sizeof(normal[0]));
    expect_tokens("whitespace separators", " \t one  two \n", whitespace,
                  sizeof(whitespace) / sizeof(whitespace[0]));
    expect_tokens("single quotes preserve literal text",
                  "echo 'a   b|c;d'", single_quotes,
                  sizeof(single_quotes) / sizeof(single_quotes[0]));
    expect_tokens("single quotes keep backslashes literal",
                  "echo 'a\\ b'", single_quote_backslash,
                  sizeof(single_quote_backslash) /
                      sizeof(single_quote_backslash[0]));
    expect_tokens("double quotes and escaping",
                  "echo \"a b|c;d>e&f\\\"g\"", double_quotes,
                  sizeof(double_quotes) / sizeof(double_quotes[0]));
    expect_tokens("escaped space and operator", "echo a\\ b \\> c", escaped,
                  sizeof(escaped) / sizeof(escaped[0]));
    expect_tokens("adjacent operators",
                  "echo x>f;cat<f>>g&echo done", adjacent_operators,
                  sizeof(adjacent_operators) / sizeof(adjacent_operators[0]));
    expect_tokens("quoted operators", "echo '|<>>;&'", quoted_operators,
                  sizeof(quoted_operators) / sizeof(quoted_operators[0]));
    expect_tokens("empty and concatenated quoted arguments",
                  "echo \"\" '' a\"b\" ''", empty_arguments,
                  sizeof(empty_arguments) / sizeof(empty_arguments[0]));
    expect_tokens("comments and hashes inside words",
                  "echo x#kept ''#also-kept # ignored", comments,
                  sizeof(comments) / sizeof(comments[0]));
    expect_tokens("comment follows a separator", "echo hi;# ignored",
                  separators_and_comments,
                  sizeof(separators_and_comments) /
                      sizeof(separators_and_comments[0]));
    expect_tokens("escaped hash", "echo \\#not-a-comment", escaped_hash,
                  sizeof(escaped_hash) / sizeof(escaped_hash[0]));
    expect_tokens("expansion remains literal", "echo '$HOME' \"${name}\"",
                  literal_expansion,
                  sizeof(literal_expansion) / sizeof(literal_expansion[0]));
    expect_expansion_mask("unquoted expansion context",
                          "echo $HOME \"$HOME\" '$HOME' \\$HOME", 1,
                          expandable, sizeof(expandable));
    expect_expansion_mask("double-quoted expansion context",
                          "echo $HOME \"$HOME\" '$HOME' \\$HOME", 2,
                          expandable, sizeof(expandable));
    expect_expansion_mask("single-quoted literal context",
                          "echo $HOME \"$HOME\" '$HOME' \\$HOME", 3,
                          literal, sizeof(literal));
    expect_expansion_mask("escaped dollar context",
                          "echo $HOME \"$HOME\" '$HOME' \\$HOME", 4,
                          escaped_dollar, sizeof(escaped_dollar));
    expect_expansion_mask("mixed quote segments",
                          "echo 'literal'$HOME", 1, mixed_quotes,
                          sizeof(mixed_quotes));
    expect_tokens("empty input", " \t\r\n", blank, 0);
    expect_tokens("comment-only input", "# ignored", blank, 0);
    expect_separate_validation();
    run_tests();

    expect_error("unterminated double quote", "echo \"unterminated",
                 "unterminated double quote");
    expect_error("syntax error discards preceding commands",
                 "echo before; echo \"oops; echo after",
                 "unterminated double quote");
    expect_error("unterminated single quote", "echo 'unterminated",
                 "unterminated single quote");
    expect_error("leading pipe", "| ls", "empty pipeline stage");
    expect_error("trailing pipe", "ls |", "empty pipeline stage");
    expect_error("doubled pipe stage", "ls | | wc", "empty pipeline stage");
    expect_error("doubled pipe", "ls ||| wc", "|| is not supported");
    expect_error("missing input target", "cat <", "missing file name");
    expect_error("missing target before separator", "echo hi > ;",
                 "missing file name");
    expect_error("redirection without command", "> lonely.txt",
                 "redirection without a command");
    expect_error("redirection in a pipeline without command", "> f | cat",
                 "redirection without a command");
    expect_error("unsupported and", "echo a && echo b",
                 "&& is not supported");
    expect_error("unsupported or", "false || echo b",
                 "|| is not supported");
    expect_error("malformed substitution", "echo ${bad", "bad substitution");
    expect_error("malformed substitution in double quotes",
                 "echo \"${bad\" # }", "bad substitution");
    expect_error("empty redirection target", "cat < \"\"", "missing file name");
    expect_error("trailing escape", "echo trailing\\", "trailing escape");

    expect_large_input();

    if (failures != 0) {
        fprintf(stderr, "%d of %d tests failed\n", failures, tests_run);
        return EXIT_FAILURE;
    }
    printf("All %d parser tests passed\n", tests_run);
    return EXIT_SUCCESS;
}
