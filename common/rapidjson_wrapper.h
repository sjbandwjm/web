#pragma once
#include "rapidjson/document.h"
#include "rapidjson/stringbuffer.h"
#include "rapidjson/writer.h"


namespace jiayou::base {
class JsonStringWriter {
 public:
  JsonStringWriter(): writer_(s_) {
    writer_.StartObject();
  }

  class Assign {
   public:
    Assign(rapidjson::Writer<rapidjson::StringBuffer>& writer, JsonStringWriter& owner): writer_(writer), owner_(owner) {}
    Assign(const Assign& other):writer_(other.writer_), owner_(other.owner_) {}

    void operator=(int val) { writer_.Int64(val); }

    void operator=(int64_t val) { writer_.Int64(val); }

    void operator=(uint64_t val) { writer_.Uint64(val); }

    void operator=(double val) { writer_.Double(val); }

    void operator=(std::string_view val) {
      writer_.String(val.data(), val.length());
    }

    void SetNull() { writer_.Null(); }

    void SetRawValue(std::string_view val);

    //void SetDoc(const ionstore::proto::Document& doc);

    template <typename fn>
    void SetArray(const fn& f) {
      writer_.StartArray();
      f(owner_);
      writer_.EndArray();
    }

    template <typename fn>
    void SetObject(const fn& f) {
      writer_.StartObject();
      f(owner_);
      writer_.EndObject();
    }

   private:
    rapidjson::Writer<rapidjson::StringBuffer>& writer_;
    JsonStringWriter& owner_;
  };

  Assign operator[](std::string_view key) {
    writer_.Key(key.data(), key.length());
    return Assign(writer_, *this);
  }

  template <typename T>
  void Append(const T& val) {
    Assign w(writer_, *this);
    w=val;
  }

  template <typename fn>
  void AppendObj(const fn& f) {
    writer_.StartObject();
    f(*this);
    writer_.EndObject();
  }

  template <typename fn>
  void AppendArray(const fn& f) {
    writer_.StartArray();
    f(*this);
    writer_.EndArray();
  }
  std::string_view GetStringView() {
    return GetString();
  }

  const char* GetString() {
    if (!isFinished_) {
      writer_.EndObject();
      isFinished_ = true;
    }
    return s_.GetString();
  }

 private:
  bool isFinished_{false};
  rapidjson::StringBuffer s_;
  rapidjson::Writer<rapidjson::StringBuffer> writer_;
};


template <typename T>
class GenericRapidJsonWrapper {
 public:
  GenericRapidJsonWrapper(const T& doc):
      doc_(&doc),
      end_iter_(doc_->MemberEnd()) {
      assert(doc_->IsObject());
  }

  void Reset(const T& v) {
    doc_ = &v;
    end_iter_ = doc_->MemberEnd();
  }

  const std::string Marshal() {
    rapidjson::StringBuffer s;
    rapidjson::Writer<rapidjson::StringBuffer> writer(s);
    doc_->Accept(writer);
    return s.GetString();
  }
  const T& doc() const { return *doc_; }

  using Iterator = typename T::ConstMemberIterator;

  template <typename V>
  class Visitor {
   public:
    Visitor(V val, bool valid): val_(val), valid_(valid) {}

    operator bool() const {
      return valid_;
    }

    V operator*() const {
      return val_;
    }

    using VPointer = typename std::remove_reference<V>::type *;
    VPointer operator->() const {
      return &val_;
    }

   private:
    V val_;
    bool valid_;
  };

  bool HasMember(const char* key) const {
    return doc_->HasMember(key);
  }

  Visitor<const T&> FindMember(const char* key) const {
    auto it = doc_->FindMember(key);
    return Visitor<const T&>(it->value, it != end_iter_ );
  }

  Visitor<int> FindMemberInt(const char* key) const {
    auto it = doc_->FindMember(key);
    bool valid = it != end_iter_ && it->value.IsInt();
    return Visitor<int>(valid ? it->value.GetInt() : 0, valid);
  }

  Visitor<int64_t> FindMemberInt64(const char* key) const {
    auto it = doc_->FindMember(key);
    bool valid = it != end_iter_ && it->value.IsInt64();
    return Visitor<int64_t>(valid ? it->value.GetInt64() : 0, valid);
  }

  Visitor<const char*> FindMemberString(const char* key) const {
    auto it = doc_->FindMember(key);
    bool valid = it != end_iter_ && it->value.IsString();
    return Visitor<const char*>(valid ? it->value.GetString() : nullptr, valid);
  }

  Visitor<bool> FindMemberBool(const char* key) const {
    auto it = doc_->FindMember(key);
    bool valid = it != end_iter_ && it->value.IsBool();
    return Visitor<uint>(valid ? it->value.GetBool() : false, valid);
  }

  Visitor<uint> FindMemberUint(const char* key) const {
    auto it = doc_->FindMember(key);
    bool valid = it != end_iter_ && it->value.IsUint();
    return Visitor<uint>(valid ? it->value.GetUint() : 0, valid);
  }

  Visitor<uint64_t> FindMemberUint64(const char* key) const {
    auto it = doc_->FindMember(key);
    bool valid = it != end_iter_ && it->value.IsUint64();
    return Visitor<uint64_t>(valid ? it->value.GetUint64() : 0, valid);
  }

  Visitor<const T&> FindMemberObject(const char* key) const {
    auto it = doc_->FindMember(key);
    bool valid = it != end_iter_ && it->value.IsObject();
    return Visitor<const T&>(it->value, valid);
  }

  Visitor<const T&> FindMemberArray(const char* key) const {
    auto it = doc_->FindMember(key);
    bool valid = it != end_iter_ && it->value.IsArray();
    return Visitor<const T&>(it->value, valid);
  }

  Visitor<const T&> FindMemberNumber(const char* key) const {
    auto it = doc_->FindMember(key);
    bool valid = it != end_iter_ && it->value.IsNumber();
    return Visitor<const T&>(it->value, valid);
  }

 private:
  const T* doc_;
  typename T::ConstMemberIterator end_iter_;
};

typedef GenericRapidJsonWrapper<rapidjson::Value> RapidJsonWrapper;

} //namespace
