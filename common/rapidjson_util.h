#include <string>
#include <rapidjson/document.h>
#include <libbson-1.0/bson.h>

namespace jiayou::base::json {

/**
 * @param doc
 * @return
 */
[[maybe_unused]] std::string Stringify(const rapidjson::Document& doc);

bool ParseFromBsonObject(rapidjson::Document& out, bson_t* bson);
bool ParseFromBsonObjectData(rapidjson::Document& out, const void* data, size_t len);

bool ParseFromBsonArray(rapidjson::Document& out, bson_t* bson);
bool ParseFromBsonArrayData(rapidjson::Document& out, const void* data, size_t len);

}
