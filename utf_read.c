#include <locale.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <wchar.h>

char *unified_byte_pointer;
char *unified_in;

char consume()
{
  return *(unified_byte_pointer++);
}

void putdata(char data)
{
  *(unified_in++) = data;
}

int read_varint()
{
  int numRead = 0;
  int result = 0;
  char currByte;
  do {
    currByte = consume();
    int value = (currByte & 0b01111111);
    result |= (value << (7 * numRead));

    numRead++;
    if (numRead > 5) {
      printf("VarInt is too big!");
      exit(1);
    }
  } while ((currByte & 0b10000000) != 0);

  return result;
}

typedef struct __varint_obj {
  char data[5];
  size_t length;
} varint_obj;

void write_varint(varint_obj* varint, int value)
{
  char* d = varint->data;
  size_t length = 0;

  do {
    char temp = (char)(value & 0b01111111);
    value >>= 7;
    if (value != 0) {
      temp |= 0b10000000;
    }
    *d = temp;
    length++;
  } while (value != 0);

  varint->length = length;
}

void utf8_raise_unexpected_end()
{
  printf("UTF-8 Parsing error: unexpected end of sequence\n");
  exit(1);
}

void encode_utf8_char(uint32_t character)
{
  if (character < 0x80)
  {
    putdata((char)character);
  }
  else if (character < 0x0800)
  {
    putdata(((character >>  6) & 0b00011111) | 0b11000000);
    putdata((character & 0b00111111) | 0b10000000);
  }
  else if (character < 0x00010000)
  {
    putdata(((character >> 12) & 0b00001111) | 0b11100000);
    putdata(((character >>  6) & 0b00111111) | 0b10000000);
    putdata(((character >>  0) & 0b00111111) | 0b10000000);
  }
  else if (character < 0x00110000)
  {
    putdata(((character >> 18) & 0b00000111) | 0b11110000);
    putdata(((character >> 12) & 0b00111111) | 0b10000000);
    putdata(((character >>  6) & 0b00111111) | 0b10000000);
    putdata(((character >>  0) & 0b00111111) | 0b10000000);
  }
}

uint32_t read_utf8()
{
  uint32_t character;
  char b = consume();
  char next;

  if ((b & 0x80) == 0)
  {
    return b;
  }
  else if ((b & 0xE0) == 0xC0) {
    character = (b & 0b00011111) << 6;

    next = consume();
    if ((next & 0xC0) == 0x80)
    {
      character |= next & 0x3F;
    }
    else {
      utf8_raise_unexpected_end();
    }
  }
  else if ((b & 0xF0) == 0xE0) {
    character = (b & 0b00001111) << 12;

    next = consume();
    if ((next & 0xC0) == 0x80)
    {
      character |= (next & 0x3F) << 6;
    }
    else {
      utf8_raise_unexpected_end();
    }

    next = consume();
    if ((next & 0xC0) == 0x80)
    {
      character |= next & 0x3F;
    }
    else {
      utf8_raise_unexpected_end();
    }
  }
  else if ((b & 0xF8) == 0xF0)
  {
    character = ((b & 0x07) << 18);

    next = consume();
    if ((next & 0xC0) == 0x80)
    {
      character |= (next & 0x3F) << 12;
    }
    else {
      utf8_raise_unexpected_end();
    }

    next = consume();
    if ((next & 0xC0) == 0x80)
    {
      character |= (next & 0x3F) << 6;
    }
    else {
      utf8_raise_unexpected_end();
    }

    next = consume();
    if ((next & 0xC0) == 0x80)
    {
      character |= next & 0x3F;
    }
    else {
      utf8_raise_unexpected_end();
    }
  }
  else {
    printf("UTF-8 Parsing error: Unknown code point\n");
    exit(1);
  }

  return character;
}

typedef struct __ucstring_t {
  uint32_t *str;
  size_t length;
} ucstring_t;

void read_string(ucstring_t* str)
{
  size_t length = read_varint();
  str->str = calloc(sizeof(uint32_t), length);

  for (size_t i = 0; i < length; i++)
  {
    uint32_t character = read_utf8();
    *(str->str+i) = character;

    if (character == 0)
    {
      printf("Warning: Possibly malformed string length\n");
      break;
    }
  }

  str->length = length;
}

size_t strlen32(uint32_t* strarg)
{
  if (!strarg)
    return -1;

    uint32_t *str = strarg;
    for (; *str; ++str);
    return str-strarg;
  }

  /*

  typedef unsigned int byte_t;

  typedef struct __ibstream_obj {
    byte_t **data;
    size_t *size;
    byte_t (*read)();
    size_t (*read_block)(byte_t*, size_t);
  } ibstream_obj;

  typedef struct __obstream_obj {
    byte_t **data;
    size_t *size;
    void (*write)(byte_t);
    size_t (*write_block)(byte_t*, size_t);
  } obstream_obj;

  typedef struct __fifo_dynamic_buffer {
    ibstream_obj istream;
    obstream_obj ostream;
  } fifo_dynamic_buffer_t;

  byte_t __fifo_dynamic_buffer_read(ibstream_obj* stream)
  {
    if (!stream->size)
      return 0;

    return *(*(stream->data)++);
  }

  size_t __fifo_dynamic_buffer_read_block(ibstream_obj* stream, byte_t* dest, size_t size)
  {
    if (size > stream->size)
      size = stream->size;

    memcpy(dest, stream->data, size);
    stream->data += size;
    
    return size;
  }

  byte_t __fifo_dynamic_buffer_write(obstream_obj* stream, byte_t data)
  {
    if (stream->size)
    {

    }
  }

  void __fifo_dynamic_buffer_init(fifo_dynamic_buffer_t* bufobj)
  {
    bufobj->data = calloc(sizeof(byte_t), 128);
    bufobj->size = 128;

    bufobj->istream.data = &bufobj->data;
    bufobj->istream.size = &bufobj->size;
    bufobj->ostream.data = &bufobj->data;
    bufobj->ostream.size = &bufobj->size;

    bufobj->istream.read = __fifo_dynamic_buffer_read;
    bufobj->istream.read_block = __fifo_dynamic_buffer_read_block;
    bufobj->ostream.write = __fifo_dynamic_buffer_write;
    bufobj->ostream.write_block = __fifo_dynamic_buffer_write_block;
  }

  */

  typedef struct __string_obj {
    int length;
    wint_t *data;
  } string_obj;

  typedef struct __stream_obj {
    char *data;
    char *read_ptr;
    char *write_ptr;
    size_t length;
    size_t capacity;
    char (*consume)(struct __stream_obj*);
    void (*write)(struct __stream_obj*, char*, size_t);
    void (*put)(struct __stream_obj*, char);
  } stream_obj;

  char __stream_obj_consume(stream_obj* stream)
  { 
    char character = *(stream->read_ptr++);
    return character;
  }

  void __stream_obj_write(stream_obj* stream, char* data, size_t length)
  {
    if (length < stream->capacity - (stream->write_ptr - stream->data))
    {
      memcpy(stream->write_ptr, data, length);
      stream->write_ptr += length;
    }
    else {
      printf("Stream error: overflow\n");
      exit(1);
    }
  }

  void __stream_obj_put(stream_obj* stream, char data)
  {
    *(stream->write_ptr++) = data; 
    stream->length++;
  }

  void __stream_obj_init(stream_obj* stream)
  {
    stream->data = calloc(sizeof(char), 128);
    stream->write_ptr = stream->data;
    stream->read_ptr = stream->data;
    stream->capacity = 128;
    stream->consume = __stream_obj_consume;
    stream->write = __stream_obj_write;
    stream->put = __stream_obj_put;
  }

void encode_and_put_utf8_char(stream_obj* stream, uint32_t character)
{
  if (character < 0x80)
  {
    stream->put(stream, (char)character);
  }
  else if (character < 0x0800)
  {
    stream->put(stream, ((character >>  6) & 0b00011111) | 0b11000000);
    stream->put(stream, (character & 0b00111111) | 0b10000000);
  }
  else if (character < 0x00010000)
  {
    stream->put(stream, ((character >> 12) & 0b00001111) | 0b11100000);
    stream->put(stream, ((character >>  6) & 0b00111111) | 0b10000000);
    stream->put(stream, ((character >>  0) & 0b00111111) | 0b10000000);
  }
  else if (character < 0x00110000)
  {
    stream->put(stream, ((character >> 18) & 0b00000111) | 0b11110000);
    stream->put(stream, ((character >> 12) & 0b00111111) | 0b10000000);
    stream->put(stream, ((character >>  6) & 0b00111111) | 0b10000000);
    stream->put(stream, ((character >>  0) & 0b00111111) | 0b10000000);
  }
}


  typedef struct __ucbstring_obj {
    uint32_t *buf;
    uint32_t *ptr;
    size_t capacity;
  } ucbstring_obj;

#define DEFAULT_UCBSTRING_CAPACITY 128

  void __ucbstring_obj_init(ucbstring_obj* strobj)
  {
    strobj->buf = calloc(sizeof(uint32_t), DEFAULT_UCBSTRING_CAPACITY);
    strobj->capacity = DEFAULT_UCBSTRING_CAPACITY;
    strobj->ptr = strobj->buf;
  }

  void __ucbstring_obj_put(ucbstring_obj* strobj, uint32_t data)
  {
    if (strobj->ptr + 1 >= strobj->buf + strobj->capacity)
    {
      size_t newcap = strobj->capacity * 2;
      size_t len = strobj->ptr - strobj->buf;
      strobj->buf = realloc(strobj->buf, sizeof(uint32_t) * newcap);
      strobj->ptr = strobj->buf + len;
      strobj->capacity = newcap;
    }

    *strobj->ptr++ = data;
  }

  void __ucbstring_obj_write(ucbstring_obj* strobj, uint32_t* data, size_t length)
  {
    if (length > strobj->capacity - (strobj->ptr - strobj->buf))
    {
      size_t newcap = strobj->capacity * 2;
      size_t len = strobj->ptr - strobj->buf;
      strobj->buf = realloc(strobj->buf, sizeof(uint32_t) * newcap);
      strobj->ptr = strobj->buf + len;
      strobj->capacity = newcap;
    }

    memcpy(strobj->ptr, data, length);
    strobj += length;
  }

  uint32_t read_utf8_stream(stream_obj* stream)
  {
    uint32_t character;
    // char b = stream->consume(stream);
    char b = __stream_obj_consume(stream);
    char next;

    if ((b & 0x80) == 0)
    {
      return b;
    }
    else if ((b & 0xE0) == 0xC0) {
      character = (b & 0b00011111) << 6;

      next = stream->consume(stream);
      if ((next & 0xC0) == 0x80)
      {
        character |= next & 0x3F;
      }
      else {
        utf8_raise_unexpected_end();
      }
    }
    else if ((b & 0xF0) == 0xE0) {
      character = (b & 0b00001111) << 12;

      next = stream->consume(stream);
      if ((next & 0xC0) == 0x80)
      {
        character |= (next & 0x3F) << 6;
      }
      else {
        utf8_raise_unexpected_end();
      }

      next = stream->consume(stream);
      if ((next & 0xC0) == 0x80)
      {
        character |= next & 0x3F;
      }
      else {
        utf8_raise_unexpected_end();
      }
    }
    else if ((b & 0xF8) == 0xF0)
    {
      character = ((b & 0x07) << 18);

      next = stream->consume(stream);
      if ((next & 0xC0) == 0x80)
      {
        character |= (next & 0x3F) << 12;
      }
      else {
        utf8_raise_unexpected_end();
      }

      next = stream->consume(stream);
      if ((next & 0xC0) == 0x80)
      {
        character |= (next & 0x3F) << 6;
      }
      else {
        utf8_raise_unexpected_end();
      }

      next = stream->consume(stream);
      if ((next & 0xC0) == 0x80)
      {
        character |= next & 0x3F;
      }
      else {
        utf8_raise_unexpected_end();
      }
    }
    else {
      printf("UTF-8 Parsing error: Unknown code point\n");
      exit(1);
    }

    return character;
  }

  int main()
  {
    setlocale(LC_ALL, "");
    
    FILE *fp = fopen("utftest.txt", "rb");

    if (!fp)
    {
      printf("Could not open file\n");
      exit(1);
    }

    size_t fsize;
    fseek(fp, 0, SEEK_END);
    fsize = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    char *data = calloc(sizeof(char), 512);
    fread(data, sizeof(char), fsize > 512 ? 512 : fsize, fp);
    unified_byte_pointer = data;

    stream_obj stream;
    __stream_obj_init(&stream);

    uint32_t ucbs_in[] = U"Лошадь или крот? Шмель или кот?";
    size_t ucbs_in_len = strlen32(ucbs_in);

    printf("[Debug] Unput UCBS: %lS\n", ucbs_in);

    for (size_t i = 0; i < ucbs_in_len; i++)
    {
      encode_and_put_utf8_char(&stream, ucbs_in[i]);
    }

    printf("[Debug] Encoded UTF-8 stream data: %s\n", stream.data);


    ucbstring_obj ucbstr;
    __ucbstring_obj_init(&ucbstr);

    for (size_t i = 0; i < ucbs_in_len; i++)
    {
      __ucbstring_obj_put(&ucbstr, read_utf8_stream(&stream));
    }
    __ucbstring_obj_put(&ucbstr, 0);

    printf("[Debug] Decoded UCBS result: %lS\n", ucbstr.buf);

  fclose(fp);

  return 0;
}
