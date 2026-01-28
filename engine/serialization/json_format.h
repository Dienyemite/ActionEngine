#pragma once

#include "serialization.h"
#include <string>

namespace action {

/*
 * JSON Format Handler - Read/Write SerialObjects as JSON
 * 
 * Simple hand-written JSON parser/writer for portability
 */

class JsonFormat {
public:
    // Convert SerialObject to JSON string
    static std::string Serialize(const SerialObject& obj, bool pretty = true);
    
    // Parse JSON string to SerialObject
    static SerialObject Deserialize(const std::string& json);
    
    // File I/O
    static bool SaveToFile(const std::string& path, const SerialObject& obj, bool pretty = true);
    static SerialObject LoadFromFile(const std::string& path);
    
private:
    // Serialization helpers
    static void WriteValue(std::stringstream& ss, const SerialValue& value, int indent, bool pretty);
    static void WriteObject(std::stringstream& ss, const SerialObject& obj, int indent, bool pretty);
    static void WriteIndent(std::stringstream& ss, int indent, bool pretty);
    static std::string EscapeString(const std::string& s);
    
    // Parsing helpers
    struct ParseContext {
        const char* data;
        size_t pos;
        size_t length;
        
        char Current() const { return pos < length ? data[pos] : '\0'; }
        char Next() { return pos < length ? data[pos++] : '\0'; }
        void Skip(size_t n = 1) { pos += n; }
        void SkipWhitespace();
        bool Match(char c);
        bool Match(const char* str);
    };
    
    static SerialValue ParseValue(ParseContext& ctx);
    static SerialObject ParseObject(ParseContext& ctx);
    static std::vector<SerialObject> ParseArray(ParseContext& ctx);
    static std::string ParseString(ParseContext& ctx);
    static f64 ParseNumber(ParseContext& ctx);
    static bool ParseBool(ParseContext& ctx);
};

} // namespace action
