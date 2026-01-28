#include "json_format.h"
#include "core/logging.h"
#include <fstream>
#include <sstream>
#include <cstdlib>
#include <cctype>

namespace action {

// ===== ParseContext =====

void JsonFormat::ParseContext::SkipWhitespace() {
    while (pos < length && std::isspace(static_cast<unsigned char>(data[pos]))) {
        ++pos;
    }
}

bool JsonFormat::ParseContext::Match(char c) {
    SkipWhitespace();
    if (Current() == c) {
        ++pos;
        return true;
    }
    return false;
}

bool JsonFormat::ParseContext::Match(const char* str) {
    SkipWhitespace();
    size_t len = std::strlen(str);
    if (pos + len <= length && std::strncmp(data + pos, str, len) == 0) {
        pos += len;
        return true;
    }
    return false;
}

// ===== Serialization =====

std::string JsonFormat::Serialize(const SerialObject& obj, bool pretty) {
    std::stringstream ss;
    WriteObject(ss, obj, 0, pretty);
    return ss.str();
}

void JsonFormat::WriteIndent(std::stringstream& ss, int indent, bool pretty) {
    if (pretty) {
        for (int i = 0; i < indent; ++i) {
            ss << "  ";
        }
    }
}

std::string JsonFormat::EscapeString(const std::string& s) {
    std::stringstream ss;
    for (char c : s) {
        switch (c) {
            case '"': ss << "\\\""; break;
            case '\\': ss << "\\\\"; break;
            case '\b': ss << "\\b"; break;
            case '\f': ss << "\\f"; break;
            case '\n': ss << "\\n"; break;
            case '\r': ss << "\\r"; break;
            case '\t': ss << "\\t"; break;
            default: ss << c; break;
        }
    }
    return ss.str();
}

void JsonFormat::WriteValue(std::stringstream& ss, const SerialValue& value, int indent, bool pretty) {
    if (std::holds_alternative<std::nullptr_t>(value)) {
        ss << "null";
    }
    else if (auto* b = std::get_if<bool>(&value)) {
        ss << (*b ? "true" : "false");
    }
    else if (auto* i = std::get_if<i64>(&value)) {
        ss << *i;
    }
    else if (auto* d = std::get_if<f64>(&value)) {
        ss << *d;
    }
    else if (auto* s = std::get_if<std::string>(&value)) {
        ss << "\"" << EscapeString(*s) << "\"";
    }
    else if (auto* arr = std::get_if<std::vector<SerialObject>>(&value)) {
        ss << "[";
        if (pretty) ss << "\n";
        for (size_t i = 0; i < arr->size(); ++i) {
            WriteIndent(ss, indent + 1, pretty);
            WriteObject(ss, (*arr)[i], indent + 1, pretty);
            if (i + 1 < arr->size()) ss << ",";
            if (pretty) ss << "\n";
        }
        WriteIndent(ss, indent, pretty);
        ss << "]";
    }
    else if (auto* obj = std::get_if<std::shared_ptr<SerialObject>>(&value)) {
        if (*obj) {
            WriteObject(ss, **obj, indent, pretty);
        } else {
            ss << "null";
        }
    }
}

void JsonFormat::WriteObject(std::stringstream& ss, const SerialObject& obj, int indent, bool pretty) {
    ss << "{";
    if (pretty) ss << "\n";
    
    bool first = true;
    
    // Write type name first if present
    if (!obj.type_name.empty()) {
        WriteIndent(ss, indent + 1, pretty);
        ss << "\"__type\": \"" << EscapeString(obj.type_name) << "\"";
        first = false;
    }
    
    // Write properties
    for (const auto& [key, value] : obj.properties) {
        if (!first) {
            ss << ",";
            if (pretty) ss << "\n";
        }
        first = false;
        
        WriteIndent(ss, indent + 1, pretty);
        ss << "\"" << EscapeString(key) << "\": ";
        WriteValue(ss, value, indent + 1, pretty);
    }
    
    if (pretty && !first) ss << "\n";
    WriteIndent(ss, indent, pretty);
    ss << "}";
}

bool JsonFormat::SaveToFile(const std::string& path, const SerialObject& obj, bool pretty) {
    std::ofstream file(path);
    if (!file.is_open()) {
        LOG_ERROR("Failed to open file for writing: {}", path);
        return false;
    }
    
    file << Serialize(obj, pretty);
    return true;
}

// ===== Deserialization =====

SerialObject JsonFormat::Deserialize(const std::string& json) {
    ParseContext ctx{json.c_str(), 0, json.length()};
    return ParseObject(ctx);
}

SerialObject JsonFormat::LoadFromFile(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        LOG_ERROR("Failed to open file for reading: {}", path);
        return {};
    }
    
    std::stringstream ss;
    ss << file.rdbuf();
    return Deserialize(ss.str());
}

SerialValue JsonFormat::ParseValue(ParseContext& ctx) {
    ctx.SkipWhitespace();
    
    char c = ctx.Current();
    
    if (c == '"') {
        return ParseString(ctx);
    }
    else if (c == '{') {
        return std::make_shared<SerialObject>(ParseObject(ctx));
    }
    else if (c == '[') {
        return ParseArray(ctx);
    }
    else if (c == 't' || c == 'f') {
        return ParseBool(ctx);
    }
    else if (c == 'n') {
        ctx.Match("null");
        return nullptr;
    }
    else if (c == '-' || std::isdigit(static_cast<unsigned char>(c))) {
        return ParseNumber(ctx);
    }
    
    LOG_ERROR("Unexpected character in JSON at position {}: {}", ctx.pos, c);
    return nullptr;
}

SerialObject JsonFormat::ParseObject(ParseContext& ctx) {
    SerialObject obj;
    
    if (!ctx.Match('{')) {
        LOG_ERROR("Expected '{{' at position {}", ctx.pos);
        return obj;
    }
    
    ctx.SkipWhitespace();
    if (ctx.Current() == '}') {
        ctx.Next();
        return obj;
    }
    
    while (true) {
        ctx.SkipWhitespace();
        
        // Parse key
        std::string key = ParseString(ctx);
        
        // Colon
        if (!ctx.Match(':')) {
            LOG_ERROR("Expected ':' after key at position {}", ctx.pos);
            break;
        }
        
        // Parse value
        SerialValue value = ParseValue(ctx);
        
        // Handle special __type key
        if (key == "__type") {
            if (auto* s = std::get_if<std::string>(&value)) {
                obj.type_name = *s;
            }
        } else {
            obj.properties[key] = value;
        }
        
        // Comma or end
        ctx.SkipWhitespace();
        if (ctx.Current() == ',') {
            ctx.Next();
        } else if (ctx.Current() == '}') {
            ctx.Next();
            break;
        } else {
            LOG_ERROR("Expected ',' or '}}' at position {}", ctx.pos);
            break;
        }
    }
    
    return obj;
}

std::vector<SerialObject> JsonFormat::ParseArray(ParseContext& ctx) {
    std::vector<SerialObject> arr;
    
    if (!ctx.Match('[')) {
        LOG_ERROR("Expected '[' at position {}", ctx.pos);
        return arr;
    }
    
    ctx.SkipWhitespace();
    if (ctx.Current() == ']') {
        ctx.Next();
        return arr;
    }
    
    while (true) {
        ctx.SkipWhitespace();
        
        // Parse object
        arr.push_back(ParseObject(ctx));
        
        // Comma or end
        ctx.SkipWhitespace();
        if (ctx.Current() == ',') {
            ctx.Next();
        } else if (ctx.Current() == ']') {
            ctx.Next();
            break;
        } else {
            LOG_ERROR("Expected ',' or ']' at position {}", ctx.pos);
            break;
        }
    }
    
    return arr;
}

std::string JsonFormat::ParseString(ParseContext& ctx) {
    ctx.SkipWhitespace();
    
    if (!ctx.Match('"')) {
        LOG_ERROR("Expected '\"' at position {}", ctx.pos);
        return "";
    }
    
    std::stringstream ss;
    while (ctx.pos < ctx.length) {
        char c = ctx.Next();
        
        if (c == '"') {
            break;
        }
        else if (c == '\\') {
            char escaped = ctx.Next();
            switch (escaped) {
                case '"': ss << '"'; break;
                case '\\': ss << '\\'; break;
                case 'b': ss << '\b'; break;
                case 'f': ss << '\f'; break;
                case 'n': ss << '\n'; break;
                case 'r': ss << '\r'; break;
                case 't': ss << '\t'; break;
                default: ss << escaped; break;
            }
        }
        else {
            ss << c;
        }
    }
    
    return ss.str();
}

f64 JsonFormat::ParseNumber(ParseContext& ctx) {
    ctx.SkipWhitespace();
    
    size_t start = ctx.pos;
    
    // Handle negative
    if (ctx.Current() == '-') ctx.Next();
    
    // Integer part
    while (std::isdigit(static_cast<unsigned char>(ctx.Current()))) ctx.Next();
    
    // Decimal part
    if (ctx.Current() == '.') {
        ctx.Next();
        while (std::isdigit(static_cast<unsigned char>(ctx.Current()))) ctx.Next();
    }
    
    // Exponent
    if (ctx.Current() == 'e' || ctx.Current() == 'E') {
        ctx.Next();
        if (ctx.Current() == '+' || ctx.Current() == '-') ctx.Next();
        while (std::isdigit(static_cast<unsigned char>(ctx.Current()))) ctx.Next();
    }
    
    std::string num_str(ctx.data + start, ctx.pos - start);
    return std::stod(num_str);
}

bool JsonFormat::ParseBool(ParseContext& ctx) {
    if (ctx.Match("true")) return true;
    if (ctx.Match("false")) return false;
    LOG_ERROR("Expected 'true' or 'false' at position {}", ctx.pos);
    return false;
}

} // namespace action
