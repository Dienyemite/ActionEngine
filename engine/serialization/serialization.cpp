#include "serialization.h"
#include "core/logging.h"

namespace action {

// ===== SerialObject =====

template<>
void SerialObject::Set<bool>(const std::string& key, const bool& value) {
    properties[key] = value;
}

template<>
void SerialObject::Set<i32>(const std::string& key, const i32& value) {
    properties[key] = static_cast<i64>(value);
}

template<>
void SerialObject::Set<i64>(const std::string& key, const i64& value) {
    properties[key] = value;
}

template<>
void SerialObject::Set<u32>(const std::string& key, const u32& value) {
    properties[key] = static_cast<i64>(value);
}

template<>
void SerialObject::Set<f32>(const std::string& key, const f32& value) {
    properties[key] = static_cast<f64>(value);
}

template<>
void SerialObject::Set<f64>(const std::string& key, const f64& value) {
    properties[key] = value;
}

template<>
void SerialObject::Set<std::string>(const std::string& key, const std::string& value) {
    properties[key] = value;
}

template<>
bool SerialObject::Get<bool>(const std::string& key, const bool& default_value) const {
    auto it = properties.find(key);
    if (it == properties.end()) return default_value;
    if (auto* v = std::get_if<bool>(&it->second)) return *v;
    return default_value;
}

template<>
i32 SerialObject::Get<i32>(const std::string& key, const i32& default_value) const {
    auto it = properties.find(key);
    if (it == properties.end()) return default_value;
    if (auto* v = std::get_if<i64>(&it->second)) return static_cast<i32>(*v);
    return default_value;
}

template<>
i64 SerialObject::Get<i64>(const std::string& key, const i64& default_value) const {
    auto it = properties.find(key);
    if (it == properties.end()) return default_value;
    if (auto* v = std::get_if<i64>(&it->second)) return *v;
    return default_value;
}

template<>
f32 SerialObject::Get<f32>(const std::string& key, const f32& default_value) const {
    auto it = properties.find(key);
    if (it == properties.end()) return default_value;
    if (auto* v = std::get_if<f64>(&it->second)) return static_cast<f32>(*v);
    return default_value;
}

template<>
f64 SerialObject::Get<f64>(const std::string& key, const f64& default_value) const {
    auto it = properties.find(key);
    if (it == properties.end()) return default_value;
    if (auto* v = std::get_if<f64>(&it->second)) return *v;
    return default_value;
}

template<>
std::string SerialObject::Get<std::string>(const std::string& key, const std::string& default_value) const {
    auto it = properties.find(key);
    if (it == properties.end()) return default_value;
    if (auto* v = std::get_if<std::string>(&it->second)) return *v;
    return default_value;
}

bool SerialObject::Has(const std::string& key) const {
    return properties.find(key) != properties.end();
}

void SerialObject::Remove(const std::string& key) {
    properties.erase(key);
}

void SerialObject::Clear() {
    properties.clear();
    type_name.clear();
}

void SerialObject::SetArray(const std::string& key, const std::vector<SerialObject>& arr) {
    properties[key] = arr;
}

std::vector<SerialObject>* SerialObject::GetArray(const std::string& key) {
    auto it = properties.find(key);
    if (it == properties.end()) return nullptr;
    return std::get_if<std::vector<SerialObject>>(&it->second);
}

const std::vector<SerialObject>* SerialObject::GetArray(const std::string& key) const {
    auto it = properties.find(key);
    if (it == properties.end()) return nullptr;
    return std::get_if<std::vector<SerialObject>>(&it->second);
}

void SerialObject::SetObject(const std::string& key, const SerialObject& obj) {
    properties[key] = std::make_shared<SerialObject>(obj);
}

SerialObject* SerialObject::GetObject(const std::string& key) {
    auto it = properties.find(key);
    if (it == properties.end()) return nullptr;
    auto* ptr = std::get_if<std::shared_ptr<SerialObject>>(&it->second);
    return ptr ? ptr->get() : nullptr;
}

const SerialObject* SerialObject::GetObject(const std::string& key) const {
    auto it = properties.find(key);
    if (it == properties.end()) return nullptr;
    auto* ptr = std::get_if<std::shared_ptr<SerialObject>>(&it->second);
    return ptr ? ptr->get() : nullptr;
}

// ===== Serializer =====

void Serializer::BeginObject(const std::string& type_name) {
    SerialObject obj;
    obj.type_name = type_name;
    m_stack.push_back(obj);
}

SerialObject Serializer::EndObject() {
    if (m_stack.empty()) {
        LOG_ERROR("EndObject called with empty stack");
        return {};
    }
    SerialObject obj = std::move(m_stack.back());
    m_stack.pop_back();
    return obj;
}

void Serializer::Write(const std::string& key, bool value) {
    if (!m_stack.empty()) Current().Set(key, value);
}

void Serializer::Write(const std::string& key, i32 value) {
    if (!m_stack.empty()) Current().Set(key, value);
}

void Serializer::Write(const std::string& key, i64 value) {
    if (!m_stack.empty()) Current().Set(key, value);
}

void Serializer::Write(const std::string& key, u32 value) {
    if (!m_stack.empty()) Current().Set(key, value);
}

void Serializer::Write(const std::string& key, u64 value) {
    if (!m_stack.empty()) Current().Set(key, static_cast<i64>(value));
}

void Serializer::Write(const std::string& key, f32 value) {
    if (!m_stack.empty()) Current().Set(key, value);
}

void Serializer::Write(const std::string& key, f64 value) {
    if (!m_stack.empty()) Current().Set(key, value);
}

void Serializer::Write(const std::string& key, const std::string& value) {
    if (!m_stack.empty()) Current().Set(key, value);
}

void Serializer::Write(const std::string& key, const char* value) {
    Write(key, std::string(value));
}

void Serializer::Write(const std::string& key, const vec2& value) {
    SerialObject obj;
    obj.type_name = "vec2";
    obj.Set("x", value.x);
    obj.Set("y", value.y);
    WriteObject(key, obj);
}

void Serializer::Write(const std::string& key, const vec3& value) {
    SerialObject obj;
    obj.type_name = "vec3";
    obj.Set("x", value.x);
    obj.Set("y", value.y);
    obj.Set("z", value.z);
    WriteObject(key, obj);
}

void Serializer::Write(const std::string& key, const vec4& value) {
    SerialObject obj;
    obj.type_name = "vec4";
    obj.Set("x", value.x);
    obj.Set("y", value.y);
    obj.Set("z", value.z);
    obj.Set("w", value.w);
    WriteObject(key, obj);
}

void Serializer::Write(const std::string& key, const quat& value) {
    SerialObject obj;
    obj.type_name = "quat";
    obj.Set("x", value.x);
    obj.Set("y", value.y);
    obj.Set("z", value.z);
    obj.Set("w", value.w);
    WriteObject(key, obj);
}

void Serializer::BeginArray(const std::string& key) {
    m_current_array_key = key;
    m_current_array.clear();
}

void Serializer::EndArray() {
    if (!m_stack.empty() && !m_current_array_key.empty()) {
        Current().SetArray(m_current_array_key, m_current_array);
    }
    m_current_array.clear();
    m_current_array_key.clear();
}

void Serializer::WriteArrayElement(const SerialObject& obj) {
    m_current_array.push_back(obj);
}

void Serializer::WriteObject(const std::string& key, const SerialObject& obj) {
    if (!m_stack.empty()) {
        Current().SetObject(key, obj);
    }
}

void Serializer::WriteResourceRef(const std::string& key, const std::string& resource_path) {
    SerialObject ref;
    ref.type_name = "ResourceRef";
    ref.Set("path", resource_path);
    WriteObject(key, ref);
}

// ===== Deserializer =====

bool Deserializer::ReadBool(const std::string& key, bool default_value) const {
    return m_current ? m_current->Get<bool>(key, default_value) : default_value;
}

i32 Deserializer::ReadInt(const std::string& key, i32 default_value) const {
    return m_current ? m_current->Get<i32>(key, default_value) : default_value;
}

i64 Deserializer::ReadInt64(const std::string& key, i64 default_value) const {
    return m_current ? m_current->Get<i64>(key, default_value) : default_value;
}

f32 Deserializer::ReadFloat(const std::string& key, f32 default_value) const {
    return m_current ? m_current->Get<f32>(key, default_value) : default_value;
}

f64 Deserializer::ReadDouble(const std::string& key, f64 default_value) const {
    return m_current ? m_current->Get<f64>(key, default_value) : default_value;
}

std::string Deserializer::ReadString(const std::string& key, const std::string& default_value) const {
    return m_current ? m_current->Get<std::string>(key, default_value) : default_value;
}

vec2 Deserializer::ReadVec2(const std::string& key, const vec2& default_value) const {
    if (!m_current) return default_value;
    auto* obj = m_current->GetObject(key);
    if (!obj) return default_value;
    return vec2{obj->Get<f32>("x", 0), obj->Get<f32>("y", 0)};
}

vec3 Deserializer::ReadVec3(const std::string& key, const vec3& default_value) const {
    if (!m_current) return default_value;
    auto* obj = m_current->GetObject(key);
    if (!obj) return default_value;
    return vec3{obj->Get<f32>("x", 0), obj->Get<f32>("y", 0), obj->Get<f32>("z", 0)};
}

vec4 Deserializer::ReadVec4(const std::string& key, const vec4& default_value) const {
    if (!m_current) return default_value;
    auto* obj = m_current->GetObject(key);
    if (!obj) return default_value;
    return vec4{obj->Get<f32>("x", 0), obj->Get<f32>("y", 0), obj->Get<f32>("z", 0), obj->Get<f32>("w", 0)};
}

quat Deserializer::ReadQuat(const std::string& key, const quat& default_value) const {
    if (!m_current) return default_value;
    auto* obj = m_current->GetObject(key);
    if (!obj) return default_value;
    return quat{obj->Get<f32>("x", 0), obj->Get<f32>("y", 0), obj->Get<f32>("z", 0), obj->Get<f32>("w", 1)};
}

bool Deserializer::HasArray(const std::string& key) const {
    return m_current && m_current->GetArray(key) != nullptr;
}

size_t Deserializer::GetArraySize(const std::string& key) const {
    if (!m_current) return 0;
    auto* arr = m_current->GetArray(key);
    return arr ? arr->size() : 0;
}

const SerialObject* Deserializer::GetArrayElement(const std::string& key, size_t index) const {
    if (!m_current) return nullptr;
    auto* arr = m_current->GetArray(key);
    if (!arr || index >= arr->size()) return nullptr;
    return &(*arr)[index];
}

bool Deserializer::HasObject(const std::string& key) const {
    return m_current && m_current->GetObject(key) != nullptr;
}

const SerialObject* Deserializer::GetObject(const std::string& key) const {
    return m_current ? m_current->GetObject(key) : nullptr;
}

std::string Deserializer::ReadResourceRef(const std::string& key) const {
    auto* obj = GetObject(key);
    if (!obj || obj->type_name != "ResourceRef") return "";
    return obj->Get<std::string>("path", "");
}

} // namespace action
