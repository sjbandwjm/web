#include "rapidjson_wrapper.h"

namespace jiayou::base {

void JsonStringWriter::Assign::SetRawValue(std::string_view val) {
  if (val.empty()) {
      writer_.Null();
  } else {
    if (writer_.RawValue(val.data(), val.size(), rapidjson::Type::kObjectType)){}
    else if (writer_.RawValue(val.data(), val.length(), rapidjson::Type::kArrayType)) {}
    else {
      writer_.String(val.data(), val.length());
    }
  }
}

} //namespace