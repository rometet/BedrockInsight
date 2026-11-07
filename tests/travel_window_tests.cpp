#include "bedrock_insight/core/travel_event.hpp"
#include "bedrock_insight/core/stats_reader.hpp"
#include "bedrock_insight/core/storage.hpp"
#include <cassert>
#include <filesystem>
#include <iostream>
#include <limits>
#include <sqlite3.h>

using namespace bedrock_insight;
constexpr std::int64_t base = 1793890800000; //2026-11-06 00:00 JST
TravelFrame frame(double x, std::int64_t us, std::int64_t ms) {
    TravelFrame f; f.x=x; f.steady_us=us; f.utc_ms=ms; return f;
}
int main() {
    {
        MovementFlags flags; flags.riding=true;
        for (auto bad : {"addon:pig", "minecraft:piglin", "minecraft:horse_fake", "addon:oak_boat", "minecraft:my_minecart"}) {
            flags.vehicle_type=bad; assert(classify_travel(flags)==TravelMode::RideOther);
        }
        flags.vehicle_type="minecraft:boat";assert(classify_travel(flags)==TravelMode::Boat);
        flags.vehicle_type="minecraft:bamboo_chest_raft";assert(classify_travel(flags)==TravelMode::Boat);
        flags={};flags.in_water=true;assert(classify_travel(flags)==TravelMode::WalkOnWater);
        flags.under_water=true;assert(classify_travel(flags)==TravelMode::WalkUnderWater);
        flags.swimming=true;assert(classify_travel(flags)==TravelMode::Swim);
        assert(travel_distance_blocks(TravelMode::Fly,0,3,4)==5);
        assert(travel_distance_blocks(TravelMode::Glide,0,3,4)==5);
        assert(travel_distance_blocks(TravelMode::Swim,0,3,4)==5);
        assert(travel_distance_blocks(TravelMode::Walk,0,3,4)==4);
        assert(travel_distance_blocks(TravelMode::RideOther,3,0,4)==0);
    }
    TravelBucket measured;
    {
        TravelWindow w; w.observe(frame(0,0,base));
        for(int n=1;n<=100;++n)w.observe(frame(static_cast<double>(n)/512.0,n*50000,base+n*50));
        assert(w.due(5000000));
        auto b=w.drain();measured=b[static_cast<std::size_t>(TravelMode::Walk)];
        // Rounding every 0.195cm observation would lose all distance. Sum first.
        assert(measured.whole_cm()==19 && measured.samples==100);
        assert(measured.first_ms==base+50 && measured.last_ms==base+5000);
        assert(!w.due(5000000));assert(w.drain()[0].whole_cm()==0);
    }
    {
        auto a=frame(0,0,base);auto b=frame(1,50000,base+50);
        TravelWindow w;w.observe(a);w.observe(b);
        for(int test=0;test<9;++test) {
            auto c=frame(2,100000,base+100);
            switch(test){
            case 0:c.dimension=1;break;
            case 1:c.mounted=true;c.vehicle_id=19;break;
            case 2:c.alive=false;break;
            case 3:c.steady_us=50000;break;
            case 4:c.steady_us=1050001;break;
            case 5:c.utc_ms=base+49;break;
            case 6:c.x=50;break;
            case 7:c.x=std::numeric_limits<double>::quiet_NaN();break;
            case 8:c.utc_ms=base+86400000;break;
            }
            assert(w.boundary(c));
        }
        assert(w.drain()[0].whole_cm()==100);
        auto boat=b;boat.mounted=true;boat.vehicle_id=55;boat.mode=TravelMode::Boat;w.reset(boat);
        boat.vehicle_id=56;boat.steady_us+=50000;boat.utc_ms+=50;assert(w.boundary(boat));
        auto invalid=boat;invalid.vehicle_id=0;assert(!TravelWindow::valid(invalid));
    }
    {
        TravelWindow w;w.observe(frame(0,0,base-100));w.observe(frame(1,50000,base-50));
        auto f=frame(2,100000,base);assert(w.boundary(f));
        auto before=w.drain()[0];assert(before.last_ms==base-50 && before.whole_cm()==100);
        w.reset(f);w.observe(frame(3,150000,base+50));assert(w.drain()[0].whole_cm()==100);
    }
    auto path=std::filesystem::temp_directory_path()/"travel_v2_fixture.sqlite3";
    assert(!std::filesystem::exists(path));
    {
        StorageWriter writer({path,64});assert(writer.start());
        Actor alice{ActorKind::Player,"uuid64:1:2","Alice"};
        auto event=travel_window_event(alice,TravelMode::Walk,measured);assert(event);
        assert(event->occurred_at_ms==base+5000 && event->amount==19);
        assert(writer.submit(*event));
        auto bad=*event;bad.details_json="{bad";assert(writer.submit(bad));
        bad=*event;bad.amount=20;assert(writer.submit(bad));
        bad=*event;bad.details_json.pop_back();bad.details_json+=",\"distance_cm\":19}";assert(writer.submit(bad));
        bad=*event;bad.occurred_at_ms+=1;assert(writer.submit(bad));
        auto legacy=*event;legacy.cause="ll26.60.poll_position_delta.v1";legacy.amount=5;
        legacy.details_json=R"({"adapter":"ServerLevelTickEvent","phase":"polled_position_delta","mode":"walk","distance_cm":5,"flush_interval_ms":5000})";
        assert(writer.submit(legacy));
        Actor bob{ActorKind::Player,"uuid64:3:4","Bob"};
        auto other=travel_window_event(bob,TravelMode::Walk,{10.0,base+10,base+20,0,2});assert(other);
        assert(writer.submit(*other));assert(writer.flush());writer.stop();
    }
    {
        StatsReader reader(path);std::string error;assert(reader.open(error));
        PlayerStatSummary personal;assert(reader.player_summary("uuid64:1:2",StatsCategory::Custom,personal,error));
        bool found=false;for(auto const& row:personal.rows)if(row.key=="minecraft:walk_one_cm"){assert(row.value==24);found=true;}
        assert(found);
        std::vector<RankingEntry> ranking;
        assert(reader.ranking(StatsCategory::Custom,"minecraft:walk_one_cm",0,10,ranking,error));
        assert(ranking.size()==2 && ranking[0].value==24 && ranking[1].value==10);
        sqlite3* db=nullptr;assert(sqlite3_open_v2(path.string().c_str(),&db,SQLITE_OPEN_READONLY,nullptr)==SQLITE_OK);
        sqlite3_stmt* check=nullptr;assert(sqlite3_prepare_v2(db,"PRAGMA integrity_check",-1,&check,nullptr)==SQLITE_OK);
        assert(sqlite3_step(check)==SQLITE_ROW);assert(std::string(reinterpret_cast<char const*>(sqlite3_column_text(check,0)))=="ok");
        sqlite3_finalize(check);sqlite3_close(db);
    }
    std::cout<<"Travel v2 PASS: native builder -> StorageWriter -> StatsReader/ranking/integrity; synthetic fixture only\n";
}
