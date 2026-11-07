#include "bedrock_insight/core/type_labels.hpp"
#include "bedrock_insight/core/stats_catalog.hpp"
#include "bedrock_insight/core/remote_api.hpp"
#include "bedrock_insight/core/storage.hpp"
#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
using namespace bedrock_insight;
int main() {
    auto asset=std::filesystem::path(__FILE__).parent_path().parent_path()/"companion/lang/ja_JP.json";
    std::string error;assert(load_type_labels(asset,error));
    assert(stat_display_label("minecraft:stone")=="石");
    assert(stat_display_label("minecraft:iron_ingot")=="鉄インゴット");
    assert(stat_display_label("minecraft:blaze")=="ブレイズ");
    assert(stat_display_label("minecraft:jump")=="ジャンプ通知（旧加速観測を含む）");
    assert(stat_display_label("addon:widget")=="addon:widget");
    auto override_file=std::filesystem::temp_directory_path()/"type-label-overrides.json";
    assert(!std::filesystem::exists(override_file));
    auto write=[&](std::string const& value){std::ofstream file(override_file,std::ios::binary);file<<value;assert(file.good());};
    auto envelope=[](std::string const& body){return "{\"schema_version\":1,\"locale\":\"ja_JP\",\"labels\":{"+body+"}}";};
    write(envelope("\"minecraft:stone\":\"星の石\",\"addon:widget\":\"追加部品\",\"minecraft:damage_taken\":\"FAKE\""));
    assert(load_type_labels(override_file,error,true));
    assert(stat_display_label("minecraft:stone")=="星の石");
    assert(stat_display_label("addon:widget")=="追加部品");
    assert(stat_display_label("minecraft:blaze")=="ブレイズ");
    assert(stat_display_label("minecraft:damage_taken")!="FAKE");
    for(auto body:{"\"bad:id:extra\":\"x\"","\"minecraft:stone\":true","\"minecraft:stone\":\"\"",
        "\"minecraft:stone\":\"a\\nb\"","\"minecraft:stone\":\"\\ud800\"",
        "\"minecraft:stone\":\"\\u0080\"","\"minecraft:stone\":\"\\u2028\"",
        "\"minecraft:stone\":\"a\",\"minecraft:stone\":\"b\""}) {
        write(envelope(body));assert(!load_type_labels(override_file,error,true));
        assert(stat_display_label("minecraft:stone")=="星の石");
    }
    for(auto raw:{"{bad","[]","{\"schema_version\":true,\"locale\":\"ja_JP\",\"labels\":{}}",
        "{\"schema_version\":1.0,\"locale\":\"ja_JP\",\"labels\":{}}",
        "{\"schema_version\":1,\"locale\":\"en_US\",\"labels\":{}}",
        "{\"schema_version\":1,\"schema_version\":1,\"locale\":\"ja_JP\",\"labels\":{}}"}) {
        write(raw);assert(!load_type_labels(override_file,error));
    }
    write(envelope("\"minecraft:stone\":\""+std::string(257,'x')+"\""));assert(!load_type_labels(override_file,error));
    write(std::string(128*1024+1,' '));assert(!load_type_labels(override_file,error));
    write(envelope("\"minecraft:stone\":\""+std::string("\xc0\xaf")+"\""));assert(!load_type_labels(override_file,error));
    write(std::string(2000,'[')+std::string(2000,']'));assert(!load_type_labels(override_file,error));
    std::string many;
    for(int i=0;i<4097;++i){if(i)many+=',';many+="\"a:k"+std::to_string(i)+"\":\"x\"";}
    write(envelope(many));assert(!load_type_labels(override_file,error));
    many.erase(many.rfind(','));write(envelope(many));
    assert(!load_type_labels(override_file,error,true)); // Base + 4096 exceeds combined budget.
    assert(stat_display_label("minecraft:stone")=="星の石");
    assert(load_type_labels(override_file,error)); // Exactly 4096 standalone entries are valid.
    assert(stat_display_label("a:k4095")=="x");
    assert(load_type_labels(asset,error));
    write(envelope("\"minecraft:stone\":\"星の石\",\"addon:widget\":\"追加部品\""));
    assert(load_type_labels(override_file,error,true));

    auto path=std::filesystem::temp_directory_path()/"type_labels_fixture.sqlite3";
    assert(!std::filesystem::exists(path));StorageWriter writer({path,64});assert(writer.start());
    for(auto item:{"minecraft:stone","addon:widget"}) for(int i=0;i<3;++i) {
        AnalyticsEvent event;event.kind=EventKind::BlockMinedResult;
        event.actor={ActorKind::Player,"private-label-player","Alice"};event.primary_type=item;
        event.cause="ll26.60.gamemode_destroy_block.v1";event.amount=1;event.occurred_at_ms=1793890801000;
        event.details_json=std::string(R"({"hook":"GameMode::$destroyBlock","phase":"return_true_post_state","scope":"same_position_block_source","before_type":")")+
            item+R"(","after_type":"minecraft:air","dimension":0,"nested":false,"state_transition_verified":true,"world_save_verified":false})";
        assert(writer.submit(event));
    }
    assert(writer.flush());writer.stop();set_remote_api_database(path);
    char buffer[8192];assert(getPlayerStats("Alice","mined",buffer,sizeof(buffer))>0);
    auto json=std::string(buffer);assert(json.find("\"total\":6")!=std::string::npos);
    assert(json.find("\"key\":\"minecraft:stone\",\"label\":\"星の石\"")!=std::string::npos);
    assert(json.find("\"key\":\"addon:widget\",\"label\":\"追加部品\"")!=std::string::npos);
    assert(json.find("private-label-player")==std::string::npos);
    assert(load_type_labels(asset,error));assert(stat_display_label("addon:widget")=="addon:widget");
    std::cout<<"Type labels PASS: shared asset, UTF-8/JSON/bounds, failed-load rollback, CUSTOM precedence, C API labels with unchanged IDs/values; synthetic only\n";
}
