#include "bedrock_insight/core/type_labels.hpp"
#include "bedrock_insight/core/json.hpp"
#include "bedrock_insight/core/stats_catalog.hpp"
#include "bedrock_insight/core/drop_observation.hpp"
#include <fstream>
#include <map>
#include <mutex>

namespace bedrock_insight {
namespace {
constexpr std::size_t kBytes=128*1024,kEntries=4096;
std::mutex labels_mutex;
std::map<std::string,std::string,std::less<>> labels;
bool valid_label(std::string_view text) {
    if(text.empty() || text.size()>256)return false;
    for(std::size_t i=0;i<text.size();) {
        auto c=static_cast<unsigned char>(text[i++]);unsigned cp=c;int more=0;
        if(c>=0xc2 && c<=0xdf){cp=c&31;more=1;}
        else if(c>=0xe0 && c<=0xef){cp=c&15;more=2;}
        else if(c>=0xf0 && c<=0xf4){cp=c&7;more=3;}
        else if(c>=0x80)return false;
        if(i+static_cast<std::size_t>(more)>text.size())return false;
        for(int n=0;n<more;++n) {
            auto next=static_cast<unsigned char>(text[i++]);if((next&0xc0)!=0x80)return false;
            cp=(cp<<6)|(next&63);
        }
        if((more==1 && cp<0x80)||(more==2 && cp<0x800)||(more==3 && cp<0x10000)
            || cp>0x10ffff || (cp>=0xd800 && cp<=0xdfff) || cp<32 || (cp>=127 && cp<=159)
            || cp==0x2028 || cp==0x2029)return false;
    }
    return true;
}
}
bool load_type_labels(std::filesystem::path const& path,std::string& error,bool merge) noexcept {
    try {
        error.clear();std::ifstream input(path,std::ios::binary);
        if(!input){error="type label file cannot be opened";return false;}
        std::string bytes(kBytes+1,'\0');input.read(bytes.data(),static_cast<std::streamsize>(bytes.size()));
        auto count=input.gcount();if(input.bad() || count>static_cast<std::streamsize>(kBytes)) {
            error="type label file exceeds read budget";return false;
        }
        bytes.resize(static_cast<std::size_t>(count));JsonValue root;
        if(parse_json(bytes,root,error,{kBytes,4,2*kEntries+16,512})!=JsonParseResult::Complete)return false;
        if(root.type()!=JsonValue::Type::Object || root.object().size()!=3){error="invalid type label document";return false;}
        auto version=root.find("schema_version"),locale=root.find("locale"),values=root.find("labels");
        if(!version || version->type()!=JsonValue::Type::Number || version->number()!="1"
            || !locale || locale->type()!=JsonValue::Type::String || locale->string()!="ja_JP"
            || !values || values->type()!=JsonValue::Type::Object || values->object().size()>kEntries) {
            error="unsupported type label schema/locale";return false;
        }
        std::map<std::string,std::string,std::less<>> pending;
        for(auto const& [key,value]:values->object()) {
            if(!valid_item_type(key) || value.type()!=JsonValue::Type::String || !valid_label(value.string())) {
                error="invalid type identifier or UTF-8 single-line label";return false;
            }
            pending.emplace(key,std::string(value.string()));
        }
        std::lock_guard lock(labels_mutex);
        if(merge) {
            auto combined=labels;
            for(auto const& [key,value]:pending)combined.insert_or_assign(key,value);
            if(combined.size()>kEntries){error="merged type labels exceed entry budget";return false;}
            labels.swap(combined);
        }else labels.swap(pending);
        return true;
    }catch(...){try{error="type label load failed";}catch(...){}return false;}
}
std::string stat_display_label(std::string_view key) {
    auto custom=custom_key_label(key);if(custom!=key)return std::string(custom);
    std::lock_guard lock(labels_mutex);
    auto it=labels.find(key);return it==labels.end() ? std::string(key) : it->second;
}
}
