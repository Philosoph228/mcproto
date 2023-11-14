#include <string.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

void error(char* msg)
{
  perror(msg);
  exit(1);
}

void utf8_raise_unexpected_end()
{
  printf("UTF-8 Parsing error: unexpected end of byte sequence\n");
  exit(1);
}

typedef struct __ucstring_t {
  uint32_t *str;
  size_t length;
} ucstring_t;

typedef struct __heap_obj {
  char* data;
  size_t length;
} heap_obj;

typedef struct __stream_obj {
  char *data;
  char *read_ptr;
  char *write_ptr;
  size_t length;
  size_t capacity;
  char (*consume)(struct __stream_obj*);
  void (*write)(struct __stream_obj*, char*, size_t);
  void (*put)(struct __stream_obj*, char);
  void (*clear)(struct __stream_obj*);
} stream_obj;

char __stream_obj_consume(stream_obj* stream)
{ 
  return *(stream->read_ptr++);
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

  if (stream->write_ptr >= stream->data + stream->capacity)
  {
    printf("[Debug][StreamObj] Expanding capacity\n");

    size_t read_pos = stream->read_ptr - stream->data;
    size_t write_pos = stream->write_ptr - stream->data;

    size_t newcap = stream->capacity * 2;
    stream->data = realloc(stream->data, newcap);
    stream->capacity = newcap;
    stream->read_ptr = stream->data + read_pos;
    stream->write_ptr = stream->data + write_pos;
  }

  *(stream->write_ptr++) = data; 
  stream->length++;
}

void __stream_obj_clear(stream_obj* stream)
{
  bzero(stream->data, stream->capacity);
  stream->read_ptr = stream->data;
  stream->write_ptr = stream->data;
}

#define STREAM_OBJ_DEFAULT_CAPACITY 256 

void __stream_obj_init(stream_obj* stream)
{
  stream->data = calloc(sizeof(char), STREAM_OBJ_DEFAULT_CAPACITY);
  stream->write_ptr = stream->data;
  stream->read_ptr = stream->data;
  stream->capacity = STREAM_OBJ_DEFAULT_CAPACITY;
  stream->consume = __stream_obj_consume;
  stream->write = __stream_obj_write;
  stream->put = __stream_obj_put;
  stream->clear = __stream_obj_clear;
}

int read_varint(stream_obj* stream)
{
  int numRead = 0;
  int result = 0;
  char currByte;
  do {
    currByte = stream->consume(stream);
    int value = (currByte & 0b01111111);
    result |= (value << (7 * numRead));

    numRead++;
    if (numRead > 5) {
      printf("VarInt Parsing errro: value is too big!");
      exit(1);
    }
  } while ((currByte & 0b10000000) != 0);

  return result;
}

typedef struct __varint_obj {
  uint8_t data[5];
  size_t size;
} varint_obj;

void write_varint(varint_obj* varint, unsigned int value)
{
  uint8_t* d = (uint8_t *)varint->data;
  size_t size = 0;

  printf("[Debug][VarInt] Source: %d\n", value);

  do {
    char temp = (uint8_t)(value & 0b01111111);
    value = value >> 7;
    if (value != 0) {
      temp |= 0b10000000;
    }
    *d = temp;
    printf("[Debug][VarInt] %d: 0x%02x\n", size, *d);
    d++;
    size++;
  } while (value != 0);


  varint->size = size;
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

void read_string(stream_obj* stream, ucstring_t* strobj)
{
  size_t length = read_varint(stream);
  printf("[Debug] String length: %d\n", length);

  uint32_t *str = (uint32_t *)calloc(sizeof(uint32_t), length + 1);

  size_t i;
  for (i = 0; i < length; ++i)
  {
    uint32_t character = read_utf8_stream(stream);
    *(str+i) = character;

    if (character == 0)
    {
      printf("Warning: Possibly malformed string length\n");
      break;
    }
  }

  *(str+i) = 0;

  strobj->length = length;
  strobj->str = str;
}

void make_string(heap_obj* heap, uint32_t* str, size_t len)
{
  stream_obj utf_stream;
  __stream_obj_init(&utf_stream);

  for (size_t i = 0; i < len; i++)
  {
    encode_and_put_utf8_char(&utf_stream, str[i]);
  }

  varint_obj len_varint;
  write_varint(&len_varint, utf_stream.write_ptr - utf_stream.read_ptr);

  heap->data = calloc(sizeof(char), len_varint.size + (utf_stream.write_ptr -
        utf_stream.read_ptr));
  

  char *heap_ptr = heap->data;
  memcpy(heap_ptr, len_varint.data, len_varint.size);
  heap_ptr += len_varint.size;
  memcpy(heap_ptr, utf_stream.read_ptr, utf_stream.write_ptr -
      utf_stream.read_ptr);
  heap_ptr += (utf_stream.write_ptr - utf_stream.read_ptr);

  free(utf_stream.data);

  heap->length = heap_ptr - heap->data;
}

void packet_pack(heap_obj* heap, char* pkt_data, size_t pkt_data_len,
    int pkt_id)
{
  printf("[Debug][Marshall] Packing data, len: %d\n", pkt_data_len);
  varint_obj pkt_id_varint;
  write_varint(&pkt_id_varint, pkt_id);

  varint_obj pkt_len_varint;
  // Write packet id VarInt + data length
  write_varint(&pkt_len_varint, pkt_id_varint.size + pkt_data_len);

  size_t pkt_pack_len = pkt_len_varint.size +
    pkt_id_varint.size +
    pkt_data_len;

  char *pkt_buffer = calloc(sizeof(char), pkt_pack_len),
    *ptr = pkt_buffer;

  // Write packet length VarInt
  memcpy(ptr, pkt_len_varint.data, pkt_len_varint.size);
  ptr += pkt_len_varint.size;

  // Write packet id VarInt
  memcpy(ptr, pkt_id_varint.data, pkt_id_varint.size);
  ptr += pkt_id_varint.size;

  // Write data
  memcpy(ptr, pkt_data, pkt_data_len);

  heap->data = pkt_buffer;
  heap->length = pkt_pack_len;
  printf("[Debug][Marshall] Result: %d\n", pkt_pack_len);
}

size_t strlen32(uint32_t* strarg)
{
  if (!strarg)
    return -1;

  uint32_t *str = strarg;
  for (; *str; ++str);
  return str-strarg;
}
   

int main(int argc, char *argv[])
{
  setlocale(LC_ALL, "");

  int sockfd, newsockfd, portno, clilen, bytes_sent;
  char buffer[256];
  struct sockaddr_in serv_addr, cli_addr;

  if (argc < 2)
  {
    fprintf(stderr, "ERROR, no port provided");
    exit(1);
  }

  sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if (sockfd < 0)
    error("ERROR opening socket");

  bzero((char *) &serv_addr, sizeof(serv_addr));
  portno = atoi(argv[1]);
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_port = htons(portno);
  serv_addr.sin_addr.s_addr = INADDR_ANY;

  if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0)
  {
    error("ERROR on binding");
  }

  listen(sockfd, 5);

  stream_obj stream;
  __stream_obj_init(&stream);

  while (1)
  {
    clilen = sizeof(cli_addr);
    newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen);
    if (newsockfd < 0)
    {
      error("ERROR on accept");
    }

    // Clear buffer before accept
    
    bzero(buffer, 256);
    bytes_sent = read(newsockfd, buffer, 255);
    if (bytes_sent < 0)
    {
      error("ERROR reading from socket");
    }
    printf("[Debug] Bytes read: %d\n", bytes_sent);

    stream.clear(&stream);
    stream.write(&stream, buffer, bytes_sent);

    int pkt_length = read_varint(&stream);
    int pkt_id = read_varint(&stream);

    printf("Length: %d, PacketID: %d\n", pkt_length, pkt_id);

    if (pkt_length > 0 && pkt_id == 0)
    {
      int protover = read_varint(&stream);
      printf("Protocol: %d (", protover);

      if (protover == 754)
      {
        printf("Minecraft 1.16.4"); 
      }
      else {
        printf("Unknown");
      }

      printf(")\n");

      ucstring_t ip;
      read_string(&stream, &ip);
      printf("Data: %ls\n", ip.str);

      uint16_t port = stream.consume(&stream) | stream.consume(&stream) << 8;
      printf("Port: %d\n", port);

      int next_state = read_varint(&stream);
      printf("Next state: %d\n", next_state);

      bzero(buffer, 256);
      bytes_sent = read(newsockfd, buffer, 255);
      if (bytes_sent < 0)
        error("ERROR reading from socket");
      printf("[Debug] Bytes read: %d\n", bytes_sent);

      stream.clear(&stream);
      stream.write(&stream, buffer, bytes_sent);

      int next_pack = read_varint(&stream);
      printf("Next pack: %d\n", next_pack);

      int next_pack_id = read_varint(&stream);
      printf("Next pack id: %d\n", next_pack_id);

      /* shutdown(newsockfd, SHUT_RD); */

      uint32_t pingjson[] = U"{\"description\":{\"extra\":[{\"text\":\""
        "English СкайБлок\"}],\"text\":\"\"},\"players\":{\"max\":0,\"online\":228,\"s"
        "ample\":[{\"id\":\"15e6ab32-e9e2-3123-a0b1-729d61a29a81\",\"name\":\"thel"
        "inx\"},{\"id\":\"18aa3d30-d0ce-3d4c-851b-469ee373c701\",\"name\":\"Sasha5"
        "67876\"},{\"id\":\"43fb25ff-cbba-3b61-8e5e-26b3458c3087\",\"name\":\"Turb"
        "o_Camry\"}]},\"version\":{\"name\":\"Paper 1.16.5\",\"protocol\":754}}";

      size_t pingjson_len = strlen32(pingjson);

      heap_obj packed_json_string;
      make_string(&packed_json_string, pingjson, strlen32(pingjson));

      heap_obj pkt;
      packet_pack(&pkt, packed_json_string.data, packed_json_string.length, 0x00);
      free(packed_json_string.data);
      
      bytes_sent = write(newsockfd, pkt.data, pkt.length);
      if (bytes_sent < 0)
        error("ERROR writing to socket");

      /* shutdown(newsockfd, SHUT_WR);

      listen(sockfd, 5);

      clilen = sizeof(cli_addr);
      newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen);
      if (newsockfd < 0)
        error("ERROR on accept");
        */

      bzero(buffer, 256);
      bytes_sent = read(newsockfd, buffer, 255);
      if (bytes_sent < 0)
        error("ERROR reading from socket");
      printf("[Debug] Bytes read: %d\n", bytes_sent);

      stream.clear(&stream);
      stream.write(&stream, buffer, bytes_sent);

      int ping_pkt_len = read_varint(&stream);
      printf("Ping pkt size: %d\n", ping_pkt_len);

      int ping_pkt_id = read_varint(&stream);
      printf("Ping pkt id: %d\n", ping_pkt_id);

      long payload = *((long *)stream.read_ptr);
      stream.read_ptr+=4;

      printf("Ping payload: %d\n", payload);

      /* shutdown(newsockfd, SHUT_RD); */

      heap_obj ping_resp_pkt;
      packet_pack(&ping_resp_pkt, (char *)&payload, sizeof(long), 1);

      write(newsockfd, ping_resp_pkt.data, ping_resp_pkt.length);
    }

    close(newsockfd);
  }

  return 0;
}
