#pragma once

#include "core/types.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <variant>
#include <memory>
#include <functional>
#include <sstream>

namespace action {

/*
 * Serialization System - Save/Load scenes and resources (Godot-style)
 * 
 * Features:
 * - JSON and binary formats
 * - Scene serialization with node hierarchy
 * - Resource references
 * - Type registration for custom types
 * - Version handling for backwards compatibility
 */

// Serializable value types
using SerialValue = std::variant<
    std::nullptr_t,
    bool,
    i64,
    f64,
    std::string,
    std::vector<struct SerialObject>,
    std::shared_ptr<struct SerialObject>
>;

// Serialized object (key-value pairs)
struct SerialObject {
    std::string type_name;
    std::unordered_map<std::string, SerialValue> properties;
    
    // Helpers
    template<typename T>
    void Set(const std::string& key, const T& value);
    
    template<typename T>
    T Get(const std::string& key, const T& default_value = T{}) const;
    
    bool Has(const std::string& key) const;
    void Remove(const std::string& key);
    void Clear();
    
    // Array helpers
    void SetArray(const std::string& key, const std::vector<SerialObject>& arr);
    std::vector<SerialObject>* GetArray(const std::string& key);
    const std::vector<SerialObject>* GetArray(const std::string& key) const;
    
    // Nested object
    void SetObject(const std::string& key, const SerialObject& obj);
    SerialObject* GetObject(const std::string& key);
    const SerialObject* GetObject(const std::string& key) const;
};

// ===== Serializer - Writing =====
class Serializer {
public:
    Serializer() = default;
    
    // Start/end object
    void BeginObject(const std::string& type_name = "");
    SerialObject EndObject();
    
    // Write primitives
    void Write(const std::string& key, bool value);
    void Write(const std::string& key, i32 value);
    void Write(const std::string& key, i64 value);
    void Write(const std::string& key, u32 value);
    void Write(const std::string& key, u64 value);
    void Write(const std::string& key, f32 value);
    void Write(const std::string& key, f64 value);
    void Write(const std::string& key, const std::string& value);
    void Write(const std::string& key, const char* value);
    
    // Write math types
    void Write(const std::string& key, const vec2& value);
    void Write(const std::string& key, const vec3& value);
    void Write(const std::string& key, const vec4& value);
    void Write(const std::string& key, const quat& value);
    
    // Write array
    void BeginArray(const std::string& key);
    void EndArray();
    void WriteArrayElement(const SerialObject& obj);
    
    // Write nested object
    void WriteObject(const std::string& key, const SerialObject& obj);
    
    // Resource reference
    void WriteResourceRef(const std::string& key, const std::string& resource_path);
    
    // Get current object
    SerialObject& Current() { return m_stack.back(); }
    
private:
    std::vector<SerialObject> m_stack;
    std::string m_current_array_key;
    std::vector<SerialObject> m_current_array;
};

// ===== Deserializer - Reading =====
class Deserializer {
public:
    Deserializer() = default;
    explicit Deserializer(const SerialObject& obj) : m_root(obj), m_current(&m_root) {}
    
    void SetRoot(const SerialObject& obj) { m_root = obj; m_current = &m_root; }
    
    // Read primitives
    bool ReadBool(const std::string& key, bool default_value = false) const;
    i32 ReadInt(const std::string& key, i32 default_value = 0) const;
    i64 ReadInt64(const std::string& key, i64 default_value = 0) const;
    f32 ReadFloat(const std::string& key, f32 default_value = 0.0f) const;
    f64 ReadDouble(const std::string& key, f64 default_value = 0.0) const;
    std::string ReadString(const std::string& key, const std::string& default_value = "") const;
    
    // Read math types
    vec2 ReadVec2(const std::string& key, const vec2& default_value = {}) const;
    vec3 ReadVec3(const std::string& key, const vec3& default_value = {}) const;
    vec4 ReadVec4(const std::string& key, const vec4& default_value = {}) const;
    quat ReadQuat(const std::string& key, const quat& default_value = {}) const;
    
    // Read array
    bool HasArray(const std::string& key) const;
    size_t GetArraySize(const std::string& key) const;
    const SerialObject* GetArrayElement(const std::string& key, size_t index) const;
    
    // Read object
    bool HasObject(const std::string& key) const;
    const SerialObject* GetObject(const std::string& key) const;
    
    // Resource reference
    std::string ReadResourceRef(const std::string& key) const;
    
    // Check existence
    bool Has(const std::string& key) const { return m_current->Has(key); }
    
    // Type name
    const std::string& GetTypeName() const { return m_current->type_name; }
    
private:
    SerialObject m_root;
    const SerialObject* m_current = nullptr;
};

} // namespace action
