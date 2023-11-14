#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

wint_t utf8_read(char* str, char** next)
{


}

typedef enum {
  JSON_TOKEN_OBJECT,
  JSON_TOKEN_STRING
} json_token_type_e;

typedef struct __json_token {
  json_token_type_e type;
  int start;
  int end;
} json_token_t;

typedef struct__json_parser_state {

} json_parser_state_t;

void json_put_token(json_token_t token, json_token_t* storage, size_t size)
{
}

int main()
{

  char sample_json[] = "{\"data\":null}";

  char* curr = sample_json;
  char* next;
  wchar_t ch = utf8_read(curr, &next);

  json_token_t[128] token_storage = {0};

  if (ch == U'{')
  {
    json_token_t token = {0};
    token->start = next;

    json_put_token(); 
  }

  curr = next;

  return EXIT_SUCCESS;
}
