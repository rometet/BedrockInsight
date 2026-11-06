#include "bedrock_insight/levilamina/stats_command.hpp"
#include "bedrock_insight/core/stats_catalog.hpp"
#include "bedrock_insight/core/stats_reader.hpp"
#include "bedrock_insight/core/stats_page.hpp"

#include "ll/api/command/CommandHandle.h"
#include "ll/api/command/CommandRegistrar.h"
#include "ll/api/form/SimpleForm.h"
#include "mc/server/commands/CommandOrigin.h"
#include "mc/server/commands/CommandOutput.h"
#include "mc/world/actor/player/Player.h"

#include <mutex>
#include <string>
#include <string_view>
#include <utility>

namespace bedrock_insight::levilamina {
// Command parameter structs need external linkage for ll reflection/pfr.
struct GuiParams final {
    std::string category;
};
struct RankParams final {
    std::string category;
    int page;
};
struct PlayerParams final {
    std::string target;
    std::string category;
};

namespace {
std::mutex g_path_mutex;
std::filesystem::path g_database_path;
bool g_registered = false;

std::filesystem::path database_path() {
    std::lock_guard lock(g_path_mutex);
    return g_database_path;
}

// The fmt-formatted CommandOutput helpers pull in fmt::vformat, which the pinned
// runtime does not export. Reply with the plain string_view overload only.
void reply(CommandOutput& output, bool ok, std::string const& text) {
    if (ok) output.success(std::string_view{text});
    else output.error(std::string_view{text});
}

StatsCategory parse_category(std::string_view value) {
    auto category = category_from_id(value);
    if (category != StatsCategory::Unknown) return category;
    for (auto const& entry : kCategories) {
        if (entry.label == value) return entry.category;
    }
    return StatsCategory::Unknown;
}

std::string category_text(StatsCategory category) {
    std::string out;
    for (auto const& entry : kCategories) {
        if (!out.empty()) out += " / ";
        if (entry.category == category) out += "[" + std::string(entry.short_id) + "]";
        else out += entry.short_id;
    }
    return out;
}

std::string summary_body(PlayerStatSummary const& summary, std::size_t page, std::size_t limit) {
    std::string body = "プレイヤー: " + summary.display_name + "\n分類: "
        + std::string(category_info(summary.category).label) + "\n合計: "
        + (summary.total_available?std::to_string(summary.total):std::string(summary.category==StatsCategory::Custom?
            "対象外（異なる単位を合算しません）":"未計測／記録なし"))
        + "\n時間値は完結セッションのみの下限・50ms換算tick。未終了／オフライン時間は除外。\n\n";
    auto const window=stats_page(summary.row_count,page,limit);
    std::size_t shown = 0;
    for (std::size_t i = 0; i < summary.rows.size() && shown < limit; ++i, ++shown) {
        body += std::string(custom_key_label(summary.rows[i].key)) + ": "
            + std::to_string(summary.rows[i].value) + "\n";
    }
    if (summary.rows.empty()) body += "この分類の記録はありません（未計測を含みます）。\n";
    body += "\nページ " + std::to_string(window.page + 1) + " / " + std::to_string(window.pages);
    body += "\n全対象種類数: "+std::to_string(summary.row_count)+"。未対応・欠測を含み、全機能受け入れ済みではありません。";
    return body;
}

void send_summary_form(Player&,PlayerStatSummary const&,std::size_t);
void reload_summary_form(Player& player,std::string const& id,StatsCategory category,std::size_t page) {
    StatsReader reader{database_path()};PlayerStatSummary summary;std::string error;
    if(page<=100000/12 && reader.open(error) && reader.player_summary(id,category,summary,error,page*12,12)) {
        auto const actual=stats_page(summary.row_count,page,12).page;
        if(actual!=page){reload_summary_form(player,id,category,actual);return;}
        send_summary_form(player,summary,page);
    } else {
        ll::form::SimpleForm failure("BedrockInsight 読取失敗","統計を取得できません。前回の値を新しい結果として表示しません。");
        failure.appendButton("閉じる",[](Player&){});failure.sendTo(player);
    }
}
void send_summary_form(Player& player, PlayerStatSummary const& summary, std::size_t page) {
    auto const window=stats_page(summary.row_count,page,12);
    page=window.page;
    ll::form::SimpleForm form("BedrockInsight 統計", summary_body(summary, page, 12));
    if(window.previous)form.appendButton("前のページ",[id=summary.player_id,category=summary.category,page](Player& target){
        reload_summary_form(target,id,category,page-1);
    });
    if(window.next)form.appendButton("次のページ",[id=summary.player_id,category=summary.category,page](Player& target){
        reload_summary_form(target,id,category,page+1);
    });
    for (auto const& entry : kCategories) {
        form.appendButton(std::string(entry.label), [player_id = summary.player_id, category = entry.category](
            Player& target) {
            reload_summary_form(target,player_id,category,0);
        });
    }
    form.sendTo(player);
}

void send_rank_form(Player&,StatsCategory,std::string const&,std::size_t);
void select_custom_rank(Player& player,std::size_t page) {
    auto const window=stats_page(kCustomKeys.size(),page,12);
    ll::form::SimpleForm form("CUSTOMキー選択","単位の異なる値を合算しません。キーがあっても収集済みとは限りません。");
    for(auto i=window.begin;i<window.end;++i) {
        auto key=std::string(kCustomKeys[i]);
        form.appendButton(std::string(custom_key_label(key)),[key](Player& target){send_rank_form(target,StatsCategory::Custom,key,0);});
    }
    if(window.previous)form.appendButton("前のキー",[page=window.page](Player& target){select_custom_rank(target,page-1);});
    if(window.next)form.appendButton("次のキー",[page=window.page](Player& target){select_custom_rank(target,page+1);});
    form.appendButton("閉じる",[](Player&){});form.sendTo(player);
}
void send_rank_form(Player& player,StatsCategory category,std::string const& key,std::size_t page) {
    StatsReader reader{database_path()};std::vector<RankingEntry> rows;std::string error;
    page=std::min<std::size_t>(page,1000);
    bool const ok=reader.open(error) && reader.ranking(category,key,page*10,11,rows,error);
    std::string body=std::string(category_info(category).label)+" / "+key+"\nページ "+std::to_string(page+1)+"\n";
    if(!ok)body+="このキーの確定統計は未対応、または読み取りに失敗しました。";
    else if(rows.empty())body+="正の記録なし（欠測を含む）。";
    else for(std::size_t i=0;i<rows.size() && i<10;++i)
        body+=std::to_string(page*10+i+1)+". "+rows[i].display_name+": "+std::to_string(rows[i].value)+"\n";
    ll::form::SimpleForm form("BedrockInsight ランキング",body);
    if(page>0)form.appendButton("前のページ",[category,key,page](Player& target){send_rank_form(target,category,key,page-1);});
    if(ok && rows.size()>10 && page<1000)form.appendButton("次のページ",[category,key,page](Player& target){send_rank_form(target,category,key,page+1);});
    form.appendButton("CUSTOMキーを選択",[](Player& target){select_custom_rank(target,0);});
    for(auto const& entry:kCategories)if(entry.category!=StatsCategory::Custom)
        form.appendButton(std::string(entry.label),[category=entry.category](Player& target){send_rank_form(target,category,{},0);});
    form.appendButton("閉じる",[](Player&){});form.sendTo(player);
}

void handle_gui(CommandOrigin const& origin, CommandOutput& output, std::string category_text_input) {
    auto* entity = origin.getEntity();
    auto* player = entity == nullptr ? nullptr : Player::tryGetFromEntity(entity->getEntityContext(), false);
    if (player == nullptr) {
        reply(output, false, "プレイヤーからの実行が必要です");
        return;
    }
    auto category = parse_category(category_text_input);
    if (category == StatsCategory::Unknown) {
        if(category_text_input.empty())category=StatsCategory::Custom;
        else {reply(output,false,"未知の統計分類です");return;}
    }
    PlayerStatSummary summary;
    summary.player_id = "uuid64:" + std::to_string(player->getUuid().a) + ":" + std::to_string(player->getUuid().b);
    summary.display_name = player->getRealName();
    summary.category = category;
    std::string error;
    StatsReader reader{database_path()};
    if (!reader.open(error) || !reader.player_summary(summary.player_id, category, summary, error,0,12)) {
        reply(output, false, "統計を読み取れません。DB未作成・予算超過・読取エラーを無記録と混同しません。");
        return;
    }
    send_summary_form(*player, summary, 0);
    reply(output, true, "統計フォームを送信しました: " + std::string(category_info(category).label));
}

void handle_rank(CommandOrigin const& origin, CommandOutput& output, std::string category_input, int page) {
    auto* entity = origin.getEntity();
    auto* player = entity == nullptr ? nullptr : Player::tryGetFromEntity(entity->getEntityContext(), false);
    std::string_view input{category_input};
    std::string custom_key;
    auto category = StatsCategory::Unknown;
    if (input.rfind("custom:", 0) == 0) {
        category = StatsCategory::Custom;
        custom_key = std::string(input.substr(7));
    } else {
        category = parse_category(input);
    }
    if (category == StatsCategory::Unknown) {
        reply(output, false, "分類が不正です。CUSTOMは custom:<key>（例 custom:minecraft:walk_one_cm）で指定します。");
        return;
    }
    if (category == StatsCategory::Custom && custom_key.empty()) {
        if(player!=nullptr){select_custom_rank(*player,0);return;}
        reply(output, false, "CUSTOMランキングには明示的なキーが必要です（例 custom:minecraft:walk_one_cm）。単位の異なる値を合算しません。");
        return;
    }
    if (page < 0) page = 0;
    if (page > 1000) page = 1000;
    if(player!=nullptr){send_rank_form(*player,category,custom_key,static_cast<std::size_t>(page));return;}
    std::string error;
    StatsReader reader{database_path()};
    std::vector<RankingEntry> rows;
    if (!reader.open(error) || !reader.ranking(category, custom_key,
            static_cast<std::size_t>(page) * 10, 10, rows, error)) {
        reply(output, false, "ランキングを取得できません: " + error);
        return;
    }
    std::string body = std::string(category_info(category).label) + " ランキング (ページ "
        + std::to_string(page + 1) + ")\n\n";
    if (rows.empty()) body += "記録がありません。\n";
    int rank = page * 10 + 1;
    for (auto const& row : rows) {
        body += std::to_string(rank++) + ". " + row.display_name + ": " + std::to_string(row.value) + "\n";
    }
    if (player != nullptr) {
        ll::form::SimpleForm form("BedrockInsight ランキング", body);
        form.appendButton("閉じる", [](Player&) {});
        form.sendTo(*player);
        reply(output, true, "ランキングを送信しました");
    } else {
        reply(output, true, body);
    }
}

void handle_player(CommandOrigin const& origin, CommandOutput& output, std::string target, std::string category_input) {
    auto* entity = origin.getEntity();
    auto* player = entity == nullptr ? nullptr : Player::tryGetFromEntity(entity->getEntityContext(), false);
    auto category = parse_category(category_input);
    if (category == StatsCategory::Unknown) {
        reply(output, false, "分類が不正です。例: minecraft:custom / mined / crafted");
        return;
    }
    std::string error;
    StatsReader reader{database_path()};
    if (!reader.open(error)) {
        reply(output, false, "DBを開けません: " + error);
        return;
    }
    std::string player_id, display_name;
    if (!reader.find_player(target, player_id, display_name, error)) {
        reply(output, false, "プレイヤーが見つかりません: " + target);
        return;
    }
    PlayerStatSummary summary;
    summary.player_id = player_id;
    summary.display_name = display_name;
    summary.category = category;
    if (!reader.player_summary(player_id, category, summary, error,0,player!=nullptr?12:20)) {
        reply(output, false, "統計を取得できません: " + error);
        return;
    }
    if (player != nullptr) {
        send_summary_form(*player, summary, 0);
        reply(output, true, display_name + " の統計を送信しました");
    } else {
        reply(output, true, display_name + ": " + summary_body(summary, 0, 20));
    }
}
} // namespace

bool register_stats_commands(std::filesystem::path database_path) noexcept {
    try {
        {
            std::lock_guard lock(g_path_mutex);
            g_database_path = std::move(database_path);
        }
        if (g_registered) return true;
        auto& registrar = ll::command::CommandRegistrar::getServerInstance();
        auto& command = registrar.getOrCreateCommand(
            "stats", "BedrockInsight 統計を表示します", CommandPermissionLevel::Any);
        command.overload<GuiParams>().text("gui").optional("category").execute(
            [](CommandOrigin const& origin, CommandOutput& output, GuiParams const& params) {
                handle_gui(origin, output, params.category);
            });
        command.overload<RankParams>().text("rank").required("category").optional("page").execute(
            [](CommandOrigin const& origin, CommandOutput& output, RankParams const& params) {
                handle_rank(origin, output, params.category, params.page);
            });
        command.overload<PlayerParams>().text("player").required("target").required("category").execute(
            [](CommandOrigin const& origin, CommandOutput& output, PlayerParams const& params) {
                handle_player(origin, output, params.target, params.category);
            });
        g_registered = true;
        return true;
    } catch (...) {
        return false;
    }
}
} // namespace bedrock_insight::levilamina
