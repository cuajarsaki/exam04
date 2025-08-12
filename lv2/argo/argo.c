#include <stdio.h>
#include <stdbool.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>

typedef struct json
{
	enum
	{
		MAP,
		INTEGER,
		STRING
	} type;
	union
	{
		struct
		{
			struct pair *data;
			size_t size;
		} map;
		int integer;
		char *string;
	};
} json;

typedef struct pair
{
	char *key;
	json value;
} pair;

// ---------------------------
// グローバルエラーフラグ
static int g_error = 0;
static int g_error_no_key = 0;

// パース関数のプロトタイプ
json parse_json(FILE *stream);
json parse_string(FILE *stream);
json parse_map(FILE *stream);
json parse_number(FILE *stream);
char *get_word(FILE *stream);
// -------------------------

int peek(FILE *stream)
{
	int c = getc(stream);
	ungetc(c, stream);
	return c;
}

void unexpected(FILE *stream)
{
	if (peek(stream) != EOF)
		printf("unexpected token '%c'\n", peek(stream));
	else
		printf("unexpected end of input\n");
}

/**
 * @brief   現在のトークンが指定の文字Cなら、getcで観測する位置を一個進むように
 * @return  成功（進む）：１、失敗（進まない）：０
 */
int accept(FILE *stream, char c)
{
	if (peek(stream) == c)
	{
		(void)getc(stream);
		return 1;
	}
	return 0;
}

int expect(FILE *stream, char c)
{
	if (accept(stream, c))
		return 1;
	unexpected(stream);
	return 0;
}

void free_json(json j)
{
	switch (j.type)
	{
	case MAP:
		for (size_t i = 0; i < j.map.size; i++)
		{
			free(j.map.data[i].key);
			free_json(j.map.data[i].value);
		}
		free(j.map.data);
		break;
	case STRING:
		free(j.string);
		break;
	default:
		break;
	}
}

void serialize(json j)
{
	switch (j.type)
	{
	case INTEGER:
		printf("%d", j.integer);
		break;
	case STRING:
		putchar('"');
		for (int i = 0; j.string[i]; i++)
		{
			if (j.string[i] == '\\' || j.string[i] == '"')
				putchar('\\');
			putchar(j.string[i]);
		}
		putchar('"');
		break;
	case MAP:
		putchar('{');
		for (size_t i = 0; i < j.map.size; i++)
		{
			if (i != 0)
				putchar(',');
			serialize((json){.type = STRING, .string = j.map.data[i].key});
			putchar(':');
			serialize(j.map.data[i].value);
		}
		putchar('}');
		break;
	}
}

// ------------------------------------------------------------------
// メイン関数: argo
int argo(json *dst, FILE *stream)
{
	if (!stream)
		return (-1);

	// グローバルエラーフラグをリセット
	g_error = 0;
	g_error_no_key = 0;

	*dst = parse_json(stream);

	if (g_error_no_key)
		return (-1);
	if (g_error)
	{
		unexpected(stream);
		return (-1);
	}
	return (1);
}

/**
 * @brief   JSON値のパース
 * @return  json
 */
json parse_json(FILE *stream)
{
	json nothing;

	// 先読みで次の文字を確認
	int next = peek(stream);

	if (next == '"') // 文字列
		return (parse_string(stream));
	if (next == '{') // マップ
		return (parse_map(stream));
	if (next == '-' || isdigit(next)) // 数値
		return (parse_number(stream));

	// エラー: 無効なトークン
	nothing.type = STRING;
	nothing.string = NULL;
	g_error = 1;
	return (nothing);
}

/**
 * @brief   文字列JSON値のパース
 * @return  json
 */
json parse_string(FILE *stream)
{
	json result;

	result.type = STRING;
	result.string = get_word(stream);

	if (g_error)
	{
		result.string = NULL;
	}

	return (result);
}

/**
 * @brief   文字列のパース
 * @return  NULL:なにもない、
 */
char *get_word(FILE *stream)
{
	char *str = NULL;
	int capacity = 16;
	int length = 0;
	int c;

	// 開始のクォートを受け入れ
	if (!accept(stream, '"'))
	{
		g_error = 1;
		return (NULL);
	}

	// 初期メモリ確保
	str = malloc(capacity);
	if (!str)
	{
		g_error = 1;
		return (NULL);
	}

	// 文字列の内容をパース
	while (peek(stream) != '"' && peek(stream) != EOF)
	{
		c = getc(stream);

		// エスケープ処理
		if (c == '\\') // バックスラッシュ文字
		{
			int next = peek(stream);
			if (next == '"' || next == '\\')
			{
				c = getc(stream); // エスケープされた文字を取得
			}
			else
			{
				g_error = 1;
				free(str);
				return (NULL);
			}
		}

		// メモリ容量チェック
		if (length >= capacity - 1)
		{
			capacity *= 2;
			str = realloc(str, capacity);
			if (!str)
			{
				g_error = 1;
				return (NULL);
			}
		}

		str[length++] = c;
	}

	// 終了のクォートを受け入れ
	if (!accept(stream, '"'))
	{
		g_error = 1;
		free(str);
		return (NULL);
	}

	str[length] = '\0';
	return (str);
}

// マップのメモリを解放する関数
void reset_map_result(json *result)
{
	if (result->map.data)
	{
		for (size_t i = 0; i < result->map.size; i++)
		{
			free(result->map.data[i].key);
			free_json(result->map.data[i].value);
		}
		free(result->map.data);
		result->map.data = NULL;
		result->map.size = 0;
	}
}

// マップのパース
json parse_map(FILE *stream)
{
	json result;
	char *key;
	json value;
	size_t capacity = 4;

	result.type = MAP;
	result.map.data = NULL;
	result.map.size = 0;

	// 開始の左中括弧を受け入れ
	if (!accept(stream, '{'))
	{
		g_error = 1;
		return (result);
	}

	// 空のマップの場合
	if (accept(stream, '}'))
		return (result);

	// 初期メモリ確保
	result.map.data = malloc(capacity * sizeof(pair));
	if (!result.map.data)
	{
		g_error = 1;
		return (result);
	}

	do
	{
		// キーをパース
		key = get_word(stream);
		if (g_error)
		{
			g_error_no_key = 1;
			reset_map_result(&result);
			return (result);
		}

		// コロンを受け入れ
		if (!accept(stream, ':'))
		{
			g_error = 1;
			free(key);
			reset_map_result(&result);
			return (result);
		}

		// 値をパース
		value = parse_json(stream);
		if (g_error)
		{
			free(key);
			reset_map_result(&result);
			return (result);
		}

		// 容量チェック
		if (result.map.size >= capacity)
		{
			capacity *= 2;
			result.map.data = realloc(result.map.data, capacity * sizeof(pair));
			if (!result.map.data)
			{
				g_error = 1;
				free(key);
				return (result);
			}
		}

		// 新しいペアを追加
		result.map.data[result.map.size].key = key;
		result.map.data[result.map.size].value = value;
		result.map.size++;

	} while (accept(stream, ','));

	// 終了の右中括弧を受け入れ
	if (!accept(stream, '}'))
	{
		g_error = 1;
		return (result);
	}

	return (result);
}

// 数値のパース
json parse_number(FILE *stream)
{
	json result;
	int number = 0;
	int sign = 1;
	int has_digits = 0;

	result.type = INTEGER;

	// マイナス符号の処理
	if (accept(stream, '-'))
		sign = -1;

	// 数字をパース
	while (isdigit(peek(stream)))
	{
		int digit = getc(stream) - '0';
		number = number * 10 + digit;
		has_digits = 1;
	}

	if (!has_digits)
	{
		g_error = 1;
		result.integer = 0;
		return (result);
	}

	result.integer = number * sign;
	return (result);
}
// ------------------------------------------------------------------

int main(int argc, char **argv)
{
	if (argc != 2)
		return 1;
	char *filename = argv[1];
	FILE *stream = fopen(filename, "r");
	json file;
	if (argo(&file, stream) != 1)
	{
		free_json(file);
		return 1;
	}
	serialize(file);
	printf("\n");
}
