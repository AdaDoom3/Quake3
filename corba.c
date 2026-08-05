#define _POSIX_C_SOURCE 200809L
#define _DEFAULT_SOURCE

#include <iso646.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>

typedef uint8_t  u8;  typedef uint16_t u16; typedef uint32_t u32; typedef uint64_t u64;
typedef int8_t   i8;  typedef int16_t  i16; typedef int32_t  i32; typedef int64_t  i64;
typedef float    f32; typedef double   f64;

typedef enum {Completed_Yes, Completed_No, Completed_Maybe} Completion_Status;
typedef struct {const char *repository_identifier; u32 minor; Completion_Status completed;} System_Exception;

System_Exception Absent_Exception (void)                    {return (System_Exception){NULL, 0, Completed_Yes};}
bool             Exception_Absent (System_Exception raised) {return raised.repository_identifier == NULL;}

#define CORBA_SYSTEM_EXCEPTIONS(Exception)                                                     \
  Exception (UNKNOWN)          Exception (BAD_PARAM)        Exception (NO_MEMORY)              \
  Exception (IMP_LIMIT)        Exception (COMM_FAILURE)     Exception (INV_OBJREF)             \
  Exception (NO_PERMISSION)    Exception (INTERNAL)         Exception (MARSHAL)                \
  Exception (INITIALIZE)       Exception (NO_IMPLEMENT)     Exception (BAD_TYPECODE)           \
  Exception (BAD_OPERATION)    Exception (NO_RESOURCES)     Exception (NO_RESPONSE)            \
  Exception (PERSIST_STORE)    Exception (BAD_INV_ORDER)    Exception (TRANSIENT)              \
  Exception (FREE_MEM)         Exception (INV_IDENT)        Exception (INV_FLAG)               \
  Exception (INTF_REPOS)       Exception (BAD_CONTEXT)      Exception (OBJ_ADAPTER)            \
  Exception (DATA_CONVERSION)  Exception (OBJECT_NOT_EXIST) Exception (TRANSACTION_REQUIRED)   \
  Exception (TRANSACTION_ROLLEDBACK)                        Exception (INVALID_TRANSACTION)

#define DEFINE_RAISE(Name)                                                                     \
  System_Exception Raise_##Name (u32 minor, Completion_Status completed)                       \
    {return (System_Exception){"IDL:omg.org/CORBA/" #Name ":1.0", minor, completed};}
CORBA_SYSTEM_EXCEPTIONS (DEFINE_RAISE)

typedef struct {u8 *data; u32 length, capacity, position; bool little_endian, overrun;} Stream;

bool Host_Is_Little_Endian (void)              {const u16 probe = 1; return *(const u8 *) &probe;}
u32  Aligned_Up (u32 offset, u32 boundary)     {return (offset + boundary - 1) / boundary * boundary;}
Stream Stream_Empty (void)                     {return (Stream){NULL, 0, 0, 0, Host_Is_Little_Endian (), false};}
void   Stream_Free  (Stream *stream)           {free (stream->data); *stream = Stream_Empty ();}

u8 *Stream_Reserve (Stream *stream, u32 amount) {
  u32 needed = stream->length + amount;
  if (needed > stream->capacity) {
    u32 grown = stream->capacity > 64 ? stream->capacity : 64;
    while (grown < needed) grown *= 2;
    stream->data     = realloc (stream->data, grown);
    stream->capacity = grown;
  }
  u8 *opening = stream->data + stream->length;
  stream->length = needed;
  return opening;
}

void Stream_Align (Stream *stream, u32 boundary) {while (stream->length % boundary) *Stream_Reserve (stream, 1) = 0;}

void Marshal_Scalar (Stream *stream, u64 value, u32 width) {
  Stream_Align (stream, width);
  u8 *opening = Stream_Reserve (stream, width);
  for (u32 index = 0; index < width; index++)
    opening[index] = (u8) (value >> 8 * (stream->little_endian ? index : width - 1 - index));
}

u64 Unmarshal_Scalar (Stream *stream, u32 width) {
  stream->position = Aligned_Up (stream->position, width);
  if (width > stream->length or stream->position > stream->length - width)
    {stream->overrun = true; stream->position = stream->length; return 0;}
  u64 value = 0;
  for (u32 index = 0; index < width; index++)
    value |= (u64) stream->data[stream->position + index] << 8 * (stream->little_endian ? index : width - 1 - index);
  stream->position += width;
  return value;
}

#define DEFINE_INTEGER_CODEC(Name, Type, Width)                                                       \
  void Marshal_##Name   (Stream *stream, Type value) {Marshal_Scalar (stream, (u64) value, Width);}   \
  Type Unmarshal_##Name (Stream *stream)             {return (Type) Unmarshal_Scalar (stream, Width);}
DEFINE_INTEGER_CODEC (Octet,              u8,   1)
DEFINE_INTEGER_CODEC (Boolean,            bool, 1)
DEFINE_INTEGER_CODEC (Character,          char, 1)
DEFINE_INTEGER_CODEC (Short,              i16,  2)
DEFINE_INTEGER_CODEC (Unsigned_Short,     u16,  2)
DEFINE_INTEGER_CODEC (Long,               i32,  4)
DEFINE_INTEGER_CODEC (Unsigned_Long,      u32,  4)
DEFINE_INTEGER_CODEC (Long_Long,          i64,  8)
DEFINE_INTEGER_CODEC (Unsigned_Long_Long, u64,  8)

#define DEFINE_REAL_CODEC(Name, Type, Carrier, Width)                                                 \
  void Marshal_##Name   (Stream *stream, Type value)                                                  \
    {Carrier held; memcpy (&held, &value, Width); Marshal_Scalar (stream, held, Width);}              \
  Type Unmarshal_##Name (Stream *stream)                                                              \
    {Carrier held = (Carrier) Unmarshal_Scalar (stream, Width);                                       \
     Type value; memcpy (&value, &held, Width); return value;}
DEFINE_REAL_CODEC (Float,  f32, u32, 4)
DEFINE_REAL_CODEC (Double, f64, u64, 8)

void Marshal_String (Stream *stream, const char *text) {
  u32 measure = (u32) strlen (text) + 1;
  Marshal_Unsigned_Long (stream, measure);
  memcpy (Stream_Reserve (stream, measure), text, measure);
}

char *Unmarshal_String (Stream *stream) {
  u32 measure = Unmarshal_Unsigned_Long (stream);
  if (measure == 0 or measure > stream->length - stream->position) {stream->overrun = true; return calloc (1, 1);}
  char *text = malloc (measure);
  memcpy (text, stream->data + stream->position, measure);
  text[measure - 1]  = 0;
  stream->position  += measure;
  return text;
}

void Marshal_Octet_Sequence (Stream *stream, const u8 *data, u32 measure) {
  Marshal_Unsigned_Long (stream, measure);
  memcpy (Stream_Reserve (stream, measure), data, measure);
}

u8 *Unmarshal_Octet_Sequence (Stream *stream, u32 *measure) {
  *measure = Unmarshal_Unsigned_Long (stream);
  if (*measure > stream->length - stream->position) {stream->overrun = true; *measure = 0;}
  u8 *data = malloc (*measure + 1);
  memcpy (data, stream->data + stream->position, *measure);
  data[*measure]     = 0;
  stream->position  += *measure;
  return data;
}

Stream Encapsulation_Begin (void) {
  Stream capsule = Stream_Empty ();
  Marshal_Boolean (&capsule, capsule.little_endian);
  return capsule;
}

void Marshal_Encapsulation (Stream *stream, Stream capsule) {
  Marshal_Octet_Sequence (stream, capsule.data, capsule.length);
  free (capsule.data);
}

Stream Unmarshal_Encapsulation (Stream *stream) {
  u32 measure = Unmarshal_Unsigned_Long (stream);
  if (measure > stream->length - stream->position) {stream->overrun = true; measure = 0;}
  Stream capsule     = {stream->data + stream->position, measure, measure, 0, true, false};
  stream->position  += measure;
  capsule.little_endian = Unmarshal_Boolean (&capsule);
  return capsule;
}

typedef struct {char *host; u16 port; u8 *object_key; u32 key_length;}                       Internet_Inter_Orb_Profile;
typedef struct {char *repository_identifier; Internet_Inter_Orb_Profile profile; int channel;} Object_Reference;

Object_Reference *Object_Create (const char *repository_identifier, const char *host,
                                 u16 port, const u8 *object_key, u32 key_length) {
  Object_Reference *object       = calloc (1, sizeof *object);
  object->repository_identifier  = strdup (repository_identifier);
  object->profile.host           = strdup (host);
  object->profile.port           = port;
  object->profile.object_key     = malloc (key_length + 1);
  object->profile.key_length     = key_length;
  object->channel                = -1;
  memcpy (object->profile.object_key, object_key, key_length);
  object->profile.object_key[key_length] = 0;
  return object;
}

bool Object_Is_Nil (const Object_Reference *object) {return object == NULL or object->profile.key_length == 0;}

Object_Reference *Object_Duplicate (const Object_Reference *object) {
  return Object_Is_Nil (object) ? NULL
       : Object_Create (object->repository_identifier, object->profile.host,
                        object->profile.port, object->profile.object_key, object->profile.key_length);
}

void Object_Release (Object_Reference *object) {
  if (object == NULL) return;
  if (object->channel >= 0) close (object->channel);
  free (object->repository_identifier);
  free (object->profile.host);
  free (object->profile.object_key);
  free (object);
}

bool Object_Is_Equivalent (const Object_Reference *left, const Object_Reference *right) {
  return Object_Is_Nil (left) or Object_Is_Nil (right)
       ? Object_Is_Nil (left) and Object_Is_Nil (right)
       : left->profile.port == right->profile.port
     and left->profile.key_length == right->profile.key_length
     and strcmp (left->profile.host, right->profile.host) == 0
     and memcmp (left->profile.object_key, right->profile.object_key, left->profile.key_length) == 0;
}

u32 Object_Hash (const Object_Reference *object, u32 maximum) {
  u32 folded = 5381;
  for (u32 index = 0; object and index < object->profile.key_length; index++)
    folded = folded * 33 + object->profile.object_key[index];
  return maximum ? folded % maximum : folded;
}

void Marshal_Object (Stream *stream, const Object_Reference *object) {
  if (Object_Is_Nil (object)) {Marshal_String (stream, ""); Marshal_Unsigned_Long (stream, 0); return;}
  Marshal_String        (stream, object->repository_identifier);
  Marshal_Unsigned_Long (stream, 1);
  Marshal_Unsigned_Long (stream, 0);
  Stream body = Encapsulation_Begin ();
  Marshal_Octet          (&body, 1);
  Marshal_Octet          (&body, 0);
  Marshal_String         (&body, object->profile.host);
  Marshal_Unsigned_Short (&body, object->profile.port);
  Marshal_Octet_Sequence (&body, object->profile.object_key, object->profile.key_length);
  Marshal_Encapsulation  (stream, body);
}

Object_Reference *Unmarshal_Object (Stream *stream) {
  char             *repository_identifier = Unmarshal_String (stream);
  u32               profiles              = Unmarshal_Unsigned_Long (stream);
  Object_Reference *object                = NULL;
  for (u32 index = 0; index < profiles and not stream->overrun; index++) {
    u32    tag  = Unmarshal_Unsigned_Long (stream);
    Stream body = Unmarshal_Encapsulation (stream);
    if (tag != 0 or object) continue;
    Unmarshal_Octet (&body);
    Unmarshal_Octet (&body);
    char *host       = Unmarshal_String (&body);
    u16   port       = Unmarshal_Unsigned_Short (&body);
    u32   key_length = 0;
    u8   *object_key = Unmarshal_Octet_Sequence (&body, &key_length);
    object = Object_Create (repository_identifier, host, port, object_key, key_length);
    free (host);
    free (object_key);
  }
  free (repository_identifier);
  return object;
}

char *Object_To_String (const Object_Reference *object) {
  Stream capsule = Encapsulation_Begin ();
  Marshal_Object (&capsule, object);
  char *text = malloc (capsule.length * 2 + 5);
  strcpy (text, "IOR:");
  for (u32 index = 0; index < capsule.length; index++)
    sprintf (text + 4 + index * 2, "%02x", capsule.data[index]);
  free (capsule.data);
  return text;
}

Object_Reference *String_To_Object (const char *text) {
  if (strncmp (text, "IOR:", 4) == 0) {
    u32    measure = (u32) strlen (text + 4) / 2;
    Stream capsule = {malloc (measure + 1), measure, measure, 0, true, false};
    for (u32 index = 0; index < measure; index++) {
      unsigned value = 0;
      sscanf (text + 4 + index * 2, "%2x", &value);
      capsule.data[index] = (u8) value;
    }
    capsule.little_endian = Unmarshal_Boolean (&capsule);
    Object_Reference *object = Unmarshal_Object (&capsule);
    free (capsule.data);
    return object;
  }
  if (strncmp (text, "corbaloc:iiop:", 14) != 0) return NULL;
  const char *at    = text + 14;
  const char *slash = strchr (at, '/');
  if (slash == NULL) return NULL;
  const char *marker = memchr (at, '@', (size_t) (slash - at));
  at = marker ? marker + 1 : at;
  const char *colon = memchr (at, ':', (size_t) (slash - at));
  char host[256]    = {0};
  u32  span         = (u32) ((colon ? colon : slash) - at);
  memcpy (host, at, span < 255 ? span : 255);
  return Object_Create ("", host, colon ? (u16) atoi (colon + 1) : 2809,
                        (const u8 *) slash + 1, (u32) strlen (slash + 1));
}

typedef enum {
  Kind_Null,      Kind_Void,           Kind_Short,       Kind_Long,      Kind_Unsigned_Short,
  Kind_Unsigned_Long, Kind_Float,      Kind_Double,      Kind_Boolean,   Kind_Character,
  Kind_Octet,     Kind_Any,            Kind_Type_Code,   Kind_Principal, Kind_Object_Reference,
  Kind_Structure, Kind_Union,          Kind_Enumeration, Kind_String,    Kind_Sequence,
  Kind_Array,     Kind_Alias,          Kind_Exception,   Kind_Long_Long, Kind_Unsigned_Long_Long,
  Kind_Long_Double, Kind_Wide_Character, Kind_Wide_String, Kind_Fixed
} Type_Code_Kind;

typedef struct Type_Code Type_Code;
struct Type_Code {
  Type_Code_Kind          kind;
  const char             *repository_identifier, *name;
  u32                     member_count, bound;
  const char      *const *member_names;
  const Type_Code *const *member_types;
  const i32              *member_labels;
  const Type_Code        *content_type, *discriminator_type;
  i32                     default_index;
};

const Type_Code
  Type_Null               = {.kind = Kind_Null},               Type_Void      = {.kind = Kind_Void},
  Type_Short              = {.kind = Kind_Short},              Type_Long      = {.kind = Kind_Long},
  Type_Unsigned_Short     = {.kind = Kind_Unsigned_Short},     Type_Float     = {.kind = Kind_Float},
  Type_Unsigned_Long      = {.kind = Kind_Unsigned_Long},      Type_Double    = {.kind = Kind_Double},
  Type_Boolean            = {.kind = Kind_Boolean},            Type_Character = {.kind = Kind_Character},
  Type_Octet              = {.kind = Kind_Octet},              Type_Any       = {.kind = Kind_Any},
  Type_Type_Code          = {.kind = Kind_Type_Code},          Type_String    = {.kind = Kind_String},
  Type_Long_Long          = {.kind = Kind_Long_Long},
  Type_Unsigned_Long_Long = {.kind = Kind_Unsigned_Long_Long},
  Type_Object_Reference   = {.kind = Kind_Object_Reference,
                             .repository_identifier = "IDL:omg.org/CORBA/Object:1.0", .name = "Object"};

typedef struct {u32 length; void *elements;}          Sequence;
typedef struct {const Type_Code *type; void *value;}  Any;

u32 Type_Alignment (const Type_Code *type);

u32 Members_Alignment (const Type_Code *type, u32 floor_alignment) {
  u32 alignment = floor_alignment;
  for (u32 index = 0; index < type->member_count; index++) {
    u32 candidate = Type_Alignment (type->member_types[index]);
    alignment     = candidate > alignment ? candidate : alignment;
  }
  return alignment;
}

u32 Type_Alignment (const Type_Code *type) {
  Type_Code_Kind kind = type->kind;
  return kind == Kind_Short     or kind == Kind_Unsigned_Short                              ? 2
       : kind == Kind_Long      or kind == Kind_Unsigned_Long
      or kind == Kind_Float     or kind == Kind_Enumeration                                 ? 4
       : kind == Kind_Long_Long or kind == Kind_Unsigned_Long_Long or kind == Kind_Double   ? 8
       : kind == Kind_String    or kind == Kind_Type_Code or kind == Kind_Object_Reference
      or kind == Kind_Sequence  or kind == Kind_Any                                         ? sizeof (void *)
       : kind == Kind_Structure or kind == Kind_Exception                                   ? Members_Alignment (type, 1)
       : kind == Kind_Union                                                                 ? Members_Alignment (type, 4)
       : kind == Kind_Array     or kind == Kind_Alias ? Type_Alignment (type->content_type)
       : 1;
}

u32 Type_Size (const Type_Code *type);

u32 Members_Span (const Type_Code *type) {
  u32 offset = 0;
  for (u32 index = 0; index < type->member_count; index++)
    offset = Aligned_Up (offset, Type_Alignment (type->member_types[index]))
           + Type_Size (type->member_types[index]);
  return offset;
}

u32 Union_Origin (const Type_Code *type) {return Aligned_Up (4, Members_Alignment (type, 1));}

u32 Union_Breadth (const Type_Code *type) {
  u32 breadth = 0;
  for (u32 index = 0; index < type->member_count; index++) {
    u32 candidate = Type_Size (type->member_types[index]);
    breadth       = candidate > breadth ? candidate : breadth;
  }
  return breadth;
}

u32 Type_Size (const Type_Code *type) {
  Type_Code_Kind kind = type->kind;
  return kind == Kind_Octet     or kind == Kind_Boolean or kind == Kind_Character            ? 1
       : kind == Kind_Short     or kind == Kind_Unsigned_Short                               ? 2
       : kind == Kind_Long      or kind == Kind_Unsigned_Long
      or kind == Kind_Float     or kind == Kind_Enumeration                                  ? 4
       : kind == Kind_Long_Long or kind == Kind_Unsigned_Long_Long or kind == Kind_Double    ? 8
       : kind == Kind_String    or kind == Kind_Type_Code or kind == Kind_Object_Reference   ? sizeof (void *)
       : kind == Kind_Sequence                                                               ? sizeof (Sequence)
       : kind == Kind_Any                                                                    ? sizeof (Any)
       : kind == Kind_Structure or kind == Kind_Exception
           ? Aligned_Up (Members_Span (type), Type_Alignment (type))
       : kind == Kind_Union
           ? Aligned_Up (Union_Origin (type) + Union_Breadth (type), Type_Alignment (type))
       : kind == Kind_Array                                                                  ? type->bound * Type_Size (type->content_type)
       : kind == Kind_Alias                                                                  ? Type_Size (type->content_type)
       : 0;
}

void Marshal_Type_Code (Stream *stream, const Type_Code *type) {
  Type_Code_Kind kind = type->kind;
  Marshal_Unsigned_Long (stream, kind);
  if (kind == Kind_String) {Marshal_Unsigned_Long (stream, type->bound); return;}
  if (kind == Kind_Sequence or kind == Kind_Array) {
    Stream parameters = Encapsulation_Begin ();
    Marshal_Type_Code     (&parameters, type->content_type);
    Marshal_Unsigned_Long (&parameters, type->bound);
    Marshal_Encapsulation (stream, parameters);
    return;
  }
  if (kind == Kind_Structure or kind == Kind_Exception) {
    Stream parameters = Encapsulation_Begin ();
    Marshal_String        (&parameters, type->repository_identifier);
    Marshal_String        (&parameters, type->name);
    Marshal_Unsigned_Long (&parameters, type->member_count);
    for (u32 index = 0; index < type->member_count; index++) {
      Marshal_String    (&parameters, type->member_names[index]);
      Marshal_Type_Code (&parameters, type->member_types[index]);
    }
    Marshal_Encapsulation (stream, parameters);
    return;
  }
  if (kind == Kind_Union) {
    Stream parameters = Encapsulation_Begin ();
    Marshal_String        (&parameters, type->repository_identifier);
    Marshal_String        (&parameters, type->name);
    Marshal_Type_Code     (&parameters, type->discriminator_type);
    Marshal_Long          (&parameters, type->default_index);
    Marshal_Unsigned_Long (&parameters, type->member_count);
    for (u32 index = 0; index < type->member_count; index++) {
      Marshal_Long      (&parameters, type->member_labels[index]);
      Marshal_String    (&parameters, type->member_names[index]);
      Marshal_Type_Code (&parameters, type->member_types[index]);
    }
    Marshal_Encapsulation (stream, parameters);
    return;
  }
  if (kind == Kind_Enumeration) {
    Stream parameters = Encapsulation_Begin ();
    Marshal_String        (&parameters, type->repository_identifier);
    Marshal_String        (&parameters, type->name);
    Marshal_Unsigned_Long (&parameters, type->member_count);
    for (u32 index = 0; index < type->member_count; index++)
      Marshal_String (&parameters, type->member_names[index]);
    Marshal_Encapsulation (stream, parameters);
    return;
  }
  if (kind == Kind_Alias) {
    Stream parameters = Encapsulation_Begin ();
    Marshal_String        (&parameters, type->repository_identifier);
    Marshal_String        (&parameters, type->name);
    Marshal_Type_Code     (&parameters, type->content_type);
    Marshal_Encapsulation (stream, parameters);
    return;
  }
  if (kind == Kind_Object_Reference) {
    Stream parameters = Encapsulation_Begin ();
    Marshal_String        (&parameters, type->repository_identifier);
    Marshal_String        (&parameters, type->name);
    Marshal_Encapsulation (stream, parameters);
    return;
  }
}

const Type_Code *Unmarshal_Type_Code (Stream *stream) {
  Type_Code_Kind kind = (Type_Code_Kind) Unmarshal_Unsigned_Long (stream);
  Type_Code     *type = calloc (1, sizeof *type);
  type->kind          = kind;
  type->default_index = -1;
  if (kind == Kind_String) {type->bound = Unmarshal_Unsigned_Long (stream); return type;}
  if (kind == Kind_Sequence or kind == Kind_Array) {
    Stream parameters  = Unmarshal_Encapsulation (stream);
    type->content_type = Unmarshal_Type_Code (&parameters);
    type->bound        = Unmarshal_Unsigned_Long (&parameters);
    return type;
  }
  if (kind == Kind_Structure or kind == Kind_Exception) {
    Stream parameters           = Unmarshal_Encapsulation (stream);
    type->repository_identifier = Unmarshal_String (&parameters);
    type->name                  = Unmarshal_String (&parameters);
    type->member_count          = Unmarshal_Unsigned_Long (&parameters);
    if (type->member_count > 4096) {type->member_count = 0; return type;}
    const char      **names = malloc (type->member_count * sizeof *names);
    const Type_Code **types = malloc (type->member_count * sizeof *types);
    for (u32 index = 0; index < type->member_count; index++) {
      names[index] = Unmarshal_String (&parameters);
      types[index] = Unmarshal_Type_Code (&parameters);
    }
    type->member_names = names;
    type->member_types = types;
    return type;
  }
  if (kind == Kind_Union) {
    Stream parameters           = Unmarshal_Encapsulation (stream);
    type->repository_identifier = Unmarshal_String (&parameters);
    type->name                  = Unmarshal_String (&parameters);
    type->discriminator_type    = Unmarshal_Type_Code (&parameters);
    type->default_index         = Unmarshal_Long (&parameters);
    type->member_count          = Unmarshal_Unsigned_Long (&parameters);
    if (type->member_count > 4096) {type->member_count = 0; return type;}
    i32              *labels = malloc (type->member_count * sizeof *labels);
    const char      **names  = malloc (type->member_count * sizeof *names);
    const Type_Code **types  = malloc (type->member_count * sizeof *types);
    for (u32 index = 0; index < type->member_count; index++) {
      labels[index] = Unmarshal_Long (&parameters);
      names[index]  = Unmarshal_String (&parameters);
      types[index]  = Unmarshal_Type_Code (&parameters);
    }
    type->member_labels = labels;
    type->member_names  = names;
    type->member_types  = types;
    return type;
  }
  if (kind == Kind_Enumeration) {
    Stream parameters           = Unmarshal_Encapsulation (stream);
    type->repository_identifier = Unmarshal_String (&parameters);
    type->name                  = Unmarshal_String (&parameters);
    type->member_count          = Unmarshal_Unsigned_Long (&parameters);
    if (type->member_count > 4096) {type->member_count = 0; return type;}
    const char **names = malloc (type->member_count * sizeof *names);
    for (u32 index = 0; index < type->member_count; index++)
      names[index] = Unmarshal_String (&parameters);
    type->member_names = names;
    return type;
  }
  if (kind == Kind_Alias) {
    Stream parameters           = Unmarshal_Encapsulation (stream);
    type->repository_identifier = Unmarshal_String (&parameters);
    type->name                  = Unmarshal_String (&parameters);
    type->content_type          = Unmarshal_Type_Code (&parameters);
    return type;
  }
  if (kind == Kind_Object_Reference) {
    Stream parameters           = Unmarshal_Encapsulation (stream);
    type->repository_identifier = Unmarshal_String (&parameters);
    type->name                  = Unmarshal_String (&parameters);
    return type;
  }
  return type;
}

void Marshal_Value (Stream *stream, const Type_Code *type, const void *value) {
  const u8      *at   = value;
  Type_Code_Kind kind = type->kind;
  if (kind == Kind_Alias) {Marshal_Value (stream, type->content_type, value); return;}
  if (kind == Kind_Octet or kind == Kind_Boolean or kind == Kind_Character)
    {Marshal_Scalar (stream, *at, 1); return;}
  if (kind == Kind_Short or kind == Kind_Unsigned_Short)
    {u16 held; memcpy (&held, at, 2); Marshal_Scalar (stream, held, 2); return;}
  if (kind == Kind_Long or kind == Kind_Unsigned_Long or kind == Kind_Float or kind == Kind_Enumeration)
    {u32 held; memcpy (&held, at, 4); Marshal_Scalar (stream, held, 4); return;}
  if (kind == Kind_Long_Long or kind == Kind_Unsigned_Long_Long or kind == Kind_Double)
    {u64 held; memcpy (&held, at, 8); Marshal_Scalar (stream, held, 8); return;}
  if (kind == Kind_String)           {Marshal_String    (stream, *(char *const *) value); return;}
  if (kind == Kind_Type_Code)        {Marshal_Type_Code (stream, *(const Type_Code *const *) value); return;}
  if (kind == Kind_Object_Reference) {Marshal_Object    (stream, *(Object_Reference *const *) value); return;}
  if (kind == Kind_Any) {
    const Any *boxed = value;
    Marshal_Type_Code (stream, boxed->type);
    Marshal_Value     (stream, boxed->type, boxed->value);
    return;
  }
  if (kind == Kind_Sequence) {
    const Sequence *sequence = value;
    Marshal_Unsigned_Long (stream, sequence->length);
    for (u32 index = 0; index < sequence->length; index++)
      Marshal_Value (stream, type->content_type,
                     (const u8 *) sequence->elements + index * Type_Size (type->content_type));
    return;
  }
  if (kind == Kind_Array) {
    for (u32 index = 0; index < type->bound; index++)
      Marshal_Value (stream, type->content_type, at + index * Type_Size (type->content_type));
    return;
  }
  if (kind == Kind_Structure or kind == Kind_Exception) {
    u32 offset = 0;
    for (u32 index = 0; index < type->member_count; index++) {
      offset = Aligned_Up (offset, Type_Alignment (type->member_types[index]));
      Marshal_Value (stream, type->member_types[index], at + offset);
      offset += Type_Size (type->member_types[index]);
    }
    return;
  }
  if (kind == Kind_Union) {
    i32 discriminant;
    memcpy (&discriminant, at, 4);
    Marshal_Scalar (stream, (u32) discriminant, 4);
    u32 chosen = type->member_count;
    for (u32 index = 0; index < type->member_count; index++)
      if (type->member_labels[index] == discriminant) {chosen = index; break;}
    if (chosen == type->member_count and type->default_index >= 0) chosen = (u32) type->default_index;
    if (chosen < type->member_count)
      Marshal_Value (stream, type->member_types[chosen], at + Union_Origin (type));
    return;
  }
}

void Unmarshal_Value (Stream *stream, const Type_Code *type, void *value) {
  u8            *at   = value;
  Type_Code_Kind kind = type->kind;
  if (kind == Kind_Alias) {Unmarshal_Value (stream, type->content_type, value); return;}
  if (kind == Kind_Octet or kind == Kind_Boolean or kind == Kind_Character)
    {*at = (u8) Unmarshal_Scalar (stream, 1); return;}
  if (kind == Kind_Short or kind == Kind_Unsigned_Short)
    {u16 held = (u16) Unmarshal_Scalar (stream, 2); memcpy (at, &held, 2); return;}
  if (kind == Kind_Long or kind == Kind_Unsigned_Long or kind == Kind_Float or kind == Kind_Enumeration)
    {u32 held = (u32) Unmarshal_Scalar (stream, 4); memcpy (at, &held, 4); return;}
  if (kind == Kind_Long_Long or kind == Kind_Unsigned_Long_Long or kind == Kind_Double)
    {u64 held = Unmarshal_Scalar (stream, 8); memcpy (at, &held, 8); return;}
  if (kind == Kind_String)           {*(char **) value            = Unmarshal_String (stream); return;}
  if (kind == Kind_Type_Code)        {*(const Type_Code **) value = Unmarshal_Type_Code (stream); return;}
  if (kind == Kind_Object_Reference) {*(Object_Reference **) value = Unmarshal_Object (stream); return;}
  if (kind == Kind_Any) {
    Any *boxed   = value;
    boxed->type  = Unmarshal_Type_Code (stream);
    boxed->value = calloc (1, Type_Size (boxed->type) ? Type_Size (boxed->type) : 1);
    Unmarshal_Value (stream, boxed->type, boxed->value);
    return;
  }
  if (kind == Kind_Sequence) {
    Sequence *sequence = value;
    sequence->length   = Unmarshal_Unsigned_Long (stream);
    if (sequence->length > stream->length - stream->position) {stream->overrun = true; sequence->length = 0;}
    sequence->elements = calloc (sequence->length ? sequence->length : 1, Type_Size (type->content_type));
    for (u32 index = 0; index < sequence->length; index++)
      Unmarshal_Value (stream, type->content_type,
                       (u8 *) sequence->elements + index * Type_Size (type->content_type));
    return;
  }
  if (kind == Kind_Array) {
    for (u32 index = 0; index < type->bound; index++)
      Unmarshal_Value (stream, type->content_type, at + index * Type_Size (type->content_type));
    return;
  }
  if (kind == Kind_Structure or kind == Kind_Exception) {
    u32 offset = 0;
    for (u32 index = 0; index < type->member_count; index++) {
      offset = Aligned_Up (offset, Type_Alignment (type->member_types[index]));
      Unmarshal_Value (stream, type->member_types[index], at + offset);
      offset += Type_Size (type->member_types[index]);
    }
    return;
  }
  if (kind == Kind_Union) {
    i32 discriminant = (i32) Unmarshal_Scalar (stream, 4);
    memcpy (at, &discriminant, 4);
    u32 chosen = type->member_count;
    for (u32 index = 0; index < type->member_count; index++)
      if (type->member_labels[index] == discriminant) {chosen = index; break;}
    if (chosen == type->member_count and type->default_index >= 0) chosen = (u32) type->default_index;
    if (chosen < type->member_count)
      Unmarshal_Value (stream, type->member_types[chosen], at + Union_Origin (type));
    return;
  }
}

typedef enum {
  Message_Request,      Message_Reply, Message_Cancel_Request,   Message_Locate_Request,
  Message_Locate_Reply, Message_Close_Connection, Message_Error, Message_Fragment
} Message_Kind;

typedef enum {Reply_No_Exception, Reply_User_Exception, Reply_System_Exception, Reply_Location_Forward} Reply_Status;

bool Transport_Send (int channel, const u8 *data, u32 measure) {
  while (measure) {
    ssize_t sent = send (channel, data, measure, 0);
    if (sent <= 0) return false;
    data    += sent;
    measure -= (u32) sent;
  }
  return true;
}

bool Transport_Receive (int channel, u8 *data, u32 measure) {
  while (measure) {
    ssize_t received = recv (channel, data, measure, 0);
    if (received <= 0) return false;
    data    += received;
    measure -= (u32) received;
  }
  return true;
}

int Transport_Connect (const char *host, u16 port) {
  char service[8];
  snprintf (service, sizeof service, "%u", port);
  struct addrinfo hints = {.ai_socktype = SOCK_STREAM}, *found = NULL;
  if (getaddrinfo (host, service, &hints, &found) or found == NULL) return -1;
  int channel = socket (found->ai_family, SOCK_STREAM, 0);
  if (channel >= 0 and connect (channel, found->ai_addr, found->ai_addrlen)) {close (channel); channel = -1;}
  freeaddrinfo (found);
  int enabled = 1;
  if (channel >= 0) setsockopt (channel, IPPROTO_TCP, TCP_NODELAY, &enabled, sizeof enabled);
  return channel;
}

int Transport_Listen (u16 *port) {
  int listener = socket (AF_INET, SOCK_STREAM, 0);
  int enabled  = 1;
  setsockopt (listener, SOL_SOCKET, SO_REUSEADDR, &enabled, sizeof enabled);
  struct sockaddr_in address = {.sin_family = AF_INET, .sin_port = htons (*port)};
  address.sin_addr.s_addr    = htonl (INADDR_ANY);
  if (bind (listener, (struct sockaddr *) &address, sizeof address) or listen (listener, 16))
    {close (listener); return -1;}
  socklen_t size = sizeof address;
  getsockname (listener, (struct sockaddr *) &address, &size);
  *port = ntohs (address.sin_port);
  return listener;
}

Stream Message_Begin (Message_Kind kind) {
  Stream message = Stream_Empty ();
  memcpy (Stream_Reserve (&message, 4), "GIOP", 4);
  Marshal_Octet         (&message, 1);
  Marshal_Octet         (&message, 0);
  Marshal_Octet         (&message, message.little_endian);
  Marshal_Octet         (&message, (u8) kind);
  Marshal_Unsigned_Long (&message, 0);
  return message;
}

bool Message_Send (int channel, Stream *message) {
  u32 body = message->length - 12;
  for (u32 index = 0; index < 4; index++)
    message->data[8 + index] = (u8) (body >> 8 * (message->little_endian ? index : 3 - index));
  return Transport_Send (channel, message->data, message->length);
}

Stream Message_Receive (int channel, Message_Kind *kind) {
  u8 header[12];
  *kind = Message_Error;
  if (not Transport_Receive (channel, header, 12) or memcmp (header, "GIOP", 4)) return Stream_Empty ();
  bool little_endian = header[6] & 1;
  u32  body          = 0;
  for (u32 index = 0; index < 4; index++)
    body |= (u32) header[8 + index] << 8 * (little_endian ? index : 3 - index);
  if (body > (1u << 26)) return Stream_Empty ();
  Stream message        = Stream_Empty ();
  message.little_endian = little_endian;
  memcpy (Stream_Reserve (&message, 12), header, 12);
  if (not Transport_Receive (channel, Stream_Reserve (&message, body), body))
    {Stream_Free (&message); return Stream_Empty ();}
  *kind            = (Message_Kind) header[7];
  message.position = 12;
  return message;
}

typedef struct {
  Object_Reference *target;
  const char       *operation;
  u32               identifier, arguments_origin;
  Stream            message, reply;
  System_Exception  exception;
} Request;

u32 Next_Request_Identifier = 1;

Request Request_Create (Object_Reference *target, const char *operation) {
  Request request = {.target = target, .operation = operation,
                     .identifier = Next_Request_Identifier++,
                     .message = Message_Begin (Message_Request), .reply = Stream_Empty (),
                     .exception = {NULL, 0, Completed_Yes}};
  Marshal_Unsigned_Long  (&request.message, 0);
  Marshal_Unsigned_Long  (&request.message, request.identifier);
  Marshal_Boolean        (&request.message, true);
  Marshal_Octet_Sequence (&request.message, target->profile.object_key, target->profile.key_length);
  Marshal_String         (&request.message, operation);
  u32 padding = (8 - (Aligned_Up (request.message.length, 4) + 4) % 8) % 8;
  Marshal_Unsigned_Long  (&request.message, padding);
  memset (Stream_Reserve (&request.message, padding), 0, padding);
  request.arguments_origin = request.message.length;
  return request;
}

void Request_Free (Request *request) {Stream_Free (&request->message); Stream_Free (&request->reply);}

void Request_Add_Argument (Request *request, const Type_Code *type, const void *value)
  {Marshal_Value (&request->message, type, value);}

void Request_Return_Value (Request *request, const Type_Code *type, void *value)
  {Unmarshal_Value (&request->reply, type, value);}

System_Exception Request_Invoke (Request *request) {
  Object_Reference *target = request->target;
  for (u32 attempt = 0; attempt < 4; attempt++) {
    if (target->channel < 0) target->channel = Transport_Connect (target->profile.host, target->profile.port);
    if (target->channel < 0) return request->exception = Raise_TRANSIENT (0, Completed_No);
    if (not Message_Send (target->channel, &request->message))
      {close (target->channel); target->channel = -1; continue;}
    Message_Kind kind  = Message_Error;
    Stream       reply = Message_Receive (target->channel, &kind);
    if (kind == Message_Close_Connection or reply.length < 12)
      {Stream_Free (&reply); close (target->channel); target->channel = -1; continue;}
    if (kind != Message_Reply)
      {Stream_Free (&reply); return request->exception = Raise_INTERNAL (0, Completed_Maybe);}
    for (u32 contexts = Unmarshal_Unsigned_Long (&reply); contexts; contexts--) {
      Unmarshal_Unsigned_Long (&reply);
      u32 discarded = 0;
      free (Unmarshal_Octet_Sequence (&reply, &discarded));
    }
    Unmarshal_Unsigned_Long (&reply);
    u32 verdict    = Unmarshal_Unsigned_Long (&reply);
    request->reply = reply;
    if (verdict == Reply_No_Exception) return request->exception = Absent_Exception ();
    if (verdict == Reply_System_Exception) {
      char *repository_identifier = Unmarshal_String (&request->reply);
      u32   minor                 = Unmarshal_Unsigned_Long (&request->reply);
      u32   completed             = Unmarshal_Unsigned_Long (&request->reply);
      return request->exception = (System_Exception){repository_identifier, minor, (Completion_Status) completed};
    }
    if (verdict == Reply_User_Exception)
      return request->exception = (System_Exception){Unmarshal_String (&request->reply), 0, Completed_Yes};
    if (verdict == Reply_Location_Forward) {
      Object_Reference *forwarded = Unmarshal_Object (&request->reply);
      Stream_Free (&request->reply);
      if (forwarded == NULL) return request->exception = Raise_INV_OBJREF (0, Completed_No);
      close (target->channel);
      target->channel = -1;
      target->profile = forwarded->profile;
      free (forwarded->repository_identifier);
      free (forwarded);
      Request renewed = Request_Create (target, request->operation);
      u32     span    = request->message.length - request->arguments_origin;
      memcpy (Stream_Reserve (&renewed.message, span),
              request->message.data + request->arguments_origin, span);
      Stream_Free (&request->message);
      request->message          = renewed.message;
      request->identifier       = renewed.identifier;
      request->arguments_origin = renewed.arguments_origin;
      continue;
    }
    return request->exception = Raise_MARSHAL (0, Completed_Maybe);
  }
  return request->exception = Raise_COMM_FAILURE (0, Completed_Maybe);
}

bool Object_Is_A (Object_Reference *object, const char *repository_identifier) {
  if (object->repository_identifier and strcmp (object->repository_identifier, repository_identifier) == 0)
    return true;
  Request request = Request_Create (object, "_is_a");
  Marshal_String (&request.message, repository_identifier);
  bool verdict = Exception_Absent (Request_Invoke (&request)) and Unmarshal_Boolean (&request.reply);
  Request_Free (&request);
  return verdict;
}

bool Object_Non_Existent (Object_Reference *object) {
  Request request = Request_Create (object, "_non_existent");
  bool verdict = Exception_Absent (Request_Invoke (&request)) ? Unmarshal_Boolean (&request.reply) : true;
  Request_Free (&request);
  return verdict;
}

typedef struct Servant        Servant;
typedef struct Server_Request Server_Request;
typedef void (*Operation_Handler) (Servant *servant, Server_Request *request);
typedef struct {const char *operation; Operation_Handler handle;} Operation_Entry;
struct Servant        {const char *repository_identifier; const Operation_Entry *operations;
                       u32 operation_count; void *state;};
struct Server_Request {Stream *arguments, *results; System_Exception raised; const char *operation;};

typedef struct {u8 identifier[8]; Servant *servant;} Active_Object;
typedef struct Object_Request_Broker Object_Request_Broker;
typedef struct {Active_Object registry[256]; u32 active_count; u64 counter;
                Object_Request_Broker *broker;} Portable_Object_Adapter;
struct Object_Request_Broker {char host[256]; u16 port; int listener; bool shutdown;
                              Portable_Object_Adapter root_adapter;};

Servant *Adapter_Find_Servant (Portable_Object_Adapter *adapter, const u8 *object_key, u32 key_length) {
  for (u32 index = 0; index < adapter->active_count; index++)
    if (key_length == 8 and memcmp (adapter->registry[index].identifier, object_key, 8) == 0)
      return adapter->registry[index].servant;
  return NULL;
}

Object_Reference *Adapter_Activate_Object (Portable_Object_Adapter *adapter, Servant *servant) {
  if (adapter->active_count >= 256) return NULL;
  Active_Object *entry    = &adapter->registry[adapter->active_count++];
  u64            selected = ++adapter->counter;
  for (u32 index = 0; index < 8; index++) entry->identifier[index] = (u8) (selected >> 8 * index);
  entry->servant = servant;
  return Object_Create (servant->repository_identifier, adapter->broker->host,
                        adapter->broker->port, entry->identifier, 8);
}

Object_Reference *Adapter_Servant_To_Reference (Portable_Object_Adapter *adapter, const Servant *servant) {
  for (u32 index = 0; index < adapter->active_count; index++)
    if (adapter->registry[index].servant == servant)
      return Object_Create (servant->repository_identifier, adapter->broker->host,
                            adapter->broker->port, adapter->registry[index].identifier, 8);
  return NULL;
}

void Adapter_Deactivate_Object (Portable_Object_Adapter *adapter, const Object_Reference *object) {
  for (u32 index = 0; index < adapter->active_count; index++)
    if (object->profile.key_length == 8
    and memcmp (adapter->registry[index].identifier, object->profile.object_key, 8) == 0)
      {adapter->registry[index] = adapter->registry[--adapter->active_count]; return;}
}

void Broker_Serve_Request (Object_Request_Broker *broker, int channel, Stream *message) {
  for (u32 contexts = Unmarshal_Unsigned_Long (message); contexts; contexts--) {
    Unmarshal_Unsigned_Long (message);
    u32 discarded = 0;
    free (Unmarshal_Octet_Sequence (message, &discarded));
  }
  u32   identifier = Unmarshal_Unsigned_Long (message);
  bool  respond    = Unmarshal_Boolean (message);
  u32   key_length = 0;
  u8   *object_key = Unmarshal_Octet_Sequence (message, &key_length);
  char *operation  = Unmarshal_String (message);
  u32   principal  = Unmarshal_Unsigned_Long (message);
  if (principal <= message->length - message->position) message->position += principal;
  Servant *servant = Adapter_Find_Servant (&broker->root_adapter, object_key, key_length);
  Stream   reply   = Message_Begin (Message_Reply);
  Marshal_Unsigned_Long (&reply, 0);
  Marshal_Unsigned_Long (&reply, identifier);
  u32 verdict_mark = reply.length;
  Marshal_Unsigned_Long (&reply, Reply_No_Exception);
  Server_Request incoming = {message, &reply, {NULL, 0, Completed_Yes}, operation};
  if      (servant == NULL) incoming.raised = Raise_OBJECT_NOT_EXIST (0, Completed_No);
  else if (strcmp (operation, "_is_a") == 0) {
    char *asked = Unmarshal_String (message);
    Marshal_Boolean (&reply, strcmp (asked, servant->repository_identifier) == 0
                          or strcmp (asked, "IDL:omg.org/CORBA/Object:1.0") == 0);
    free (asked);
  }
  else if (strcmp (operation, "_non_existent") == 0) Marshal_Boolean (&reply, false);
  else {
    Operation_Handler handling = NULL;
    for (u32 index = 0; index < servant->operation_count; index++)
      if (strcmp (servant->operations[index].operation, operation) == 0)
        handling = servant->operations[index].handle;
    if (handling) handling (servant, &incoming);
    else          incoming.raised = Raise_BAD_OPERATION (0, Completed_No);
  }
  if (incoming.raised.repository_identifier) {
    reply.length = verdict_mark;
    bool system  = strncmp (incoming.raised.repository_identifier, "IDL:omg.org/CORBA/", 18) == 0;
    Marshal_Unsigned_Long (&reply, system ? Reply_System_Exception : Reply_User_Exception);
    Marshal_String        (&reply, incoming.raised.repository_identifier);
    if (system) {
      Marshal_Unsigned_Long (&reply, incoming.raised.minor);
      Marshal_Unsigned_Long (&reply, incoming.raised.completed);
    }
  }
  if (respond) Message_Send (channel, &reply);
  Stream_Free (&reply);
  free (object_key);
  free (operation);
}

void Broker_Serve_Locate (Object_Request_Broker *broker, int channel, Stream *message) {
  u32   identifier = Unmarshal_Unsigned_Long (message);
  u32   key_length = 0;
  u8   *object_key = Unmarshal_Octet_Sequence (message, &key_length);
  Stream reply     = Message_Begin (Message_Locate_Reply);
  Marshal_Unsigned_Long (&reply, identifier);
  Marshal_Unsigned_Long (&reply, Adapter_Find_Servant (&broker->root_adapter, object_key, key_length) ? 1 : 0);
  Message_Send (channel, &reply);
  Stream_Free  (&reply);
  free (object_key);
}

void Broker_Serve_Connection (Object_Request_Broker *broker, int channel) {
  while (not broker->shutdown) {
    Message_Kind kind    = Message_Error;
    Stream       message = Message_Receive (channel, &kind);
    if (message.length < 12) {Stream_Free (&message); break;}
    bool closing = kind == Message_Close_Connection or kind == Message_Error;
    if      (kind == Message_Request)        Broker_Serve_Request (broker, channel, &message);
    else if (kind == Message_Locate_Request) Broker_Serve_Locate  (broker, channel, &message);
    Stream_Free (&message);
    if (closing) break;
  }
  close (channel);
}

Object_Request_Broker *ORB_Init (int *count, char **arguments, const char *identifier) {
  (void) identifier;
  Object_Request_Broker *broker = calloc (1, sizeof *broker);
  snprintf (broker->host, sizeof broker->host, "127.0.0.1");
  for (int index = 1; count and arguments and index < *count - 1; index++)
    if      (strcmp (arguments[index], "-ORBEndpointHost") == 0)
      snprintf (broker->host, sizeof broker->host, "%s", arguments[index + 1]);
    else if (strcmp (arguments[index], "-ORBEndpointPort") == 0)
      broker->port = (u16) atoi (arguments[index + 1]);
  broker->listener           = Transport_Listen (&broker->port);
  broker->root_adapter.broker = broker;
  return broker;
}

void *ORB_Resolve_Initial_References (Object_Request_Broker *broker, const char *identifier) {
  return strcmp (identifier, "RootPOA") == 0 ? (void *) &broker->root_adapter : NULL;
}

void ORB_Perform_Work (Object_Request_Broker *broker) {
  int channel = accept (broker->listener, NULL, NULL);
  if (channel >= 0) Broker_Serve_Connection (broker, channel);
}

void ORB_Run (Object_Request_Broker *broker) {
  while (not broker->shutdown) {
    int channel = accept (broker->listener, NULL, NULL);
    if (channel < 0) break;
    Broker_Serve_Connection (broker, channel);
  }
}

void ORB_Shutdown (Object_Request_Broker *broker) {broker->shutdown = true; close (broker->listener);}
void ORB_Destroy  (Object_Request_Broker *broker) {free (broker);}

#ifdef CORBA_SELF_TEST

#include <signal.h>
#include <sys/wait.h>

void Echo_Reverse (Servant *servant, Server_Request *request) {
  (void) servant;
  char *text    = Unmarshal_String (request->arguments);
  u32   measure = (u32) strlen (text);
  for (u32 index = 0; index < measure / 2; index++) {
    char kept                 = text[index];
    text[index]               = text[measure - 1 - index];
    text[measure - 1 - index] = kept;
  }
  Marshal_String (request->results, text);
  free (text);
}

void Echo_Grieve (Servant *servant, Server_Request *request) {
  (void) servant;
  request->raised = (System_Exception){"IDL:Echo/Grievance:1.0", 0, Completed_Yes};
}

const Operation_Entry Echo_Operations[] = {{"reverse", Echo_Reverse}, {"grieve", Echo_Grieve}};
Servant               Echo_Servant     = {"IDL:Echo:1.0", Echo_Operations, 2, NULL};

const Type_Code *const Point_Member_Types[] = {&Type_String, &Type_Double, &Type_Double};
const char      *const Point_Member_Names[] = {"label", "abscissa", "ordinate"};
const Type_Code        Type_Point           =
  {.kind = Kind_Structure, .repository_identifier = "IDL:Point:1.0", .name = "Point",
   .member_count = 3, .member_names = Point_Member_Names, .member_types = Point_Member_Types,
   .default_index = -1};

typedef struct {char *label; f64 abscissa, ordinate;} Point;

bool Local_Round_Trip_Passes (void) {
  Point  origin  = {"origin", 2.625, -0.125};
  Any    boxed   = {&Type_Point, &origin};
  Stream scratch = Stream_Empty ();
  Marshal_Value (&scratch, &Type_Any, &boxed);
  scratch.position = 0;
  Any unboxed = {NULL, NULL};
  Unmarshal_Value (&scratch, &Type_Any, &unboxed);
  Point *landed  = unboxed.value;
  bool   passing = unboxed.type and unboxed.type->kind == Kind_Structure
               and unboxed.type->member_count == 3
               and strcmp (landed->label, "origin") == 0
               and landed->abscissa == 2.625 and landed->ordinate == -0.125
               and not scratch.overrun;
  Stream_Free (&scratch);
  return passing;
}

int main (void) {
  int relay[2];
  if (pipe (relay)) return 1;
  pid_t child = fork ();
  if (child == 0) {
    Object_Request_Broker   *broker    = ORB_Init (NULL, NULL, "corba.c");
    Portable_Object_Adapter *adapter   = ORB_Resolve_Initial_References (broker, "RootPOA");
    Object_Reference        *object    = Adapter_Activate_Object (adapter, &Echo_Servant);
    char                    *reference = Object_To_String (object);
    ssize_t                  told      = write (relay[1], reference, strlen (reference) + 1);
    if (told > 0) ORB_Run (broker);
    _exit (0);
  }
  char reference[4096] = {0};
  if (read (relay[0], reference, sizeof reference - 1) <= 0) return 1;
  Object_Reference *echo    = String_To_Object (reference);
  bool              passing = echo != NULL and not Object_Is_Nil (echo);
  passing = passing and Object_Is_A (echo, "IDL:Echo:1.0");
  passing = passing and not Object_Non_Existent (echo);
  Request reversal = Request_Create (echo, "reverse");
  Marshal_String (&reversal.message, "corba");
  passing = passing and Exception_Absent (Request_Invoke (&reversal));
  char *reversed = Unmarshal_String (&reversal.reply);
  passing = passing and strcmp (reversed, "abroc") == 0;
  Request_Free (&reversal);
  Request grievance = Request_Create (echo, "grieve");
  passing = passing and strcmp (Request_Invoke (&grievance).repository_identifier
                                ? grievance.exception.repository_identifier : "",
                                "IDL:Echo/Grievance:1.0") == 0;
  Request_Free (&grievance);
  Request absent = Request_Create (echo, "absent");
  passing = passing and strcmp (Request_Invoke (&absent).repository_identifier
                                ? absent.exception.repository_identifier : "",
                                "IDL:omg.org/CORBA/BAD_OPERATION:1.0") == 0;
  Request_Free (&absent);
  passing = passing and Local_Round_Trip_Passes ();
  char *restrung = Object_To_String (echo);
  Object_Reference *twin = String_To_Object (restrung);
  passing = passing and twin and Object_Is_Equivalent (echo, twin);
  free (reversed);
  free (restrung);
  kill (child, SIGKILL);
  waitpid (child, NULL, 0);
  puts (passing ? "PASS" : "FAIL");
  return not passing;
}

#endif
