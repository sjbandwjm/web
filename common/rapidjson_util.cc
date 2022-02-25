#include "rapidjson_util.h"
#include "rapidjson/document.h"
#include <libbson-1.0/bson.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>

#include <butil/logging.h>

namespace {

template <class Allocator>
bool ParseObject(rapidjson::Value& obj, bson_iter_t* it, Allocator& allocator);

template <class Allocator>
bool ParseArray(rapidjson::Value& obj, bson_iter_t* it, Allocator& allocator);

template <class Allocator>
bool ParseValue(rapidjson::Value& v, bson_iter_t* it, Allocator& allocator) {
  auto value = bson_iter_value(it);
  auto type = bson_iter_type(it);
  switch(type) {
    case BSON_TYPE_BOOL:
      v.SetBool(value->value.v_bool);
      break;
    case BSON_TYPE_INT32:
      v.SetInt(value->value.v_int32);
      break;
    case BSON_TYPE_INT64:
      v.SetInt64(value->value.v_int64);
      break;
    case BSON_TYPE_DOUBLE:
      v.SetDouble(value->value.v_double);
      break;
    case BSON_TYPE_UTF8:
      v.SetString(value->value.v_utf8.str, value->value.v_utf8.len);
      break;
    case BSON_TYPE_DOCUMENT:
    case BSON_TYPE_ARRAY: {
      auto data = value->value.v_doc;
      bson_t bson;
      bson_iter_t it2;
      if (!bson_init_static(&bson, data.data, data.data_len) || !bson_iter_init(&it2, &bson)) {
        return false;
      }
      return type == BSON_TYPE_ARRAY ? ParseArray(v, &it2, allocator) : ParseObject(v, &it2, allocator);
    }
    default:
      LOG(INFO) << "Unsupported bson type: " << type;
      break;
  }
  return true;
}

template <class Allocator>
bool ParseObject(rapidjson::Value& obj, bson_iter_t* it, Allocator& allocator) {
  obj.SetObject();

  while (bson_iter_next(it)) {
    rapidjson::Value v;
    if (!ParseValue(v, it, allocator)) {
      return false;
    }

    if (!v.IsNull()) {
      rapidjson::Value k;
      k.SetString(bson_iter_key(it), allocator);
      obj.AddMember(k.Move(), v.Move(), allocator);
    }
  }

  return true;
}

template <class Allocator>
bool ParseArray(rapidjson::Value& arr, bson_iter_t* it, Allocator& allocator) {
  arr.SetArray();

  while (bson_iter_next(it)) {
    rapidjson::Value v;

    if (!ParseValue(v, it, allocator)) {
      return false;
    }

    if (!v.IsNull()) {
      arr.PushBack(v.Move(), allocator);
    }
  }

  return true;
}

}

namespace jiayou::base::json {

std::string Stringify(const rapidjson::Document& doc) {
  rapidjson::StringBuffer buffer;
  rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
  doc.Accept(writer);
  return buffer.GetString();
}

bool ParseFromBsonObject(rapidjson::Document& doc, bson_t* bson) {
  bson_iter_t it;
  if (!bson_iter_init(&it, bson)) {
    return false;
  }

  rapidjson::Document new_doc;
  if (!ParseObject(new_doc, &it, doc.GetAllocator())) {
    return false;
  }
  doc.Swap(new_doc);
  return true;
}

bool ParseFromBsonObjectData(rapidjson::Document& doc, const void* data, size_t len) {
  bson_t bson;
  if (!bson_init_static(&bson, (const uint8_t*)data, len)) {
    return false;
  }

  return ParseFromBsonObject(doc, &bson);
}

bool ParseFromBsonArray(rapidjson::Document& doc, bson_t* bson) {
  bson_iter_t it;
  if (!bson_iter_init(&it, bson)) {
    return false;
  }

  rapidjson::Document new_doc;

  if (!ParseArray(new_doc, &it, doc.GetAllocator())) {
    return false;
  }

  doc.Swap(new_doc);
  return true;
}

bool ParseFromBsonArrayData(rapidjson::Document& doc, const void* data, size_t len) {
  bson_t bson;
  if (!bson_init_static(&bson, (const uint8_t*)data, len)) {
    return false;
  }

  return ParseFromBsonArray(doc, &bson);
}

}

