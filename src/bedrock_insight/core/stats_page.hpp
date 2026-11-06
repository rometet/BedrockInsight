#pragma once
#include <algorithm>
#include <cstddef>

namespace bedrock_insight {
struct StatsPage final {std::size_t page,begin,end,pages;bool previous,next;};
inline StatsPage stats_page(std::size_t count,std::size_t requested,std::size_t limit) noexcept {
    limit=std::clamp<std::size_t>(limit,1,100);
    auto pages=count==0?1:1+(count-1)/limit;
    auto page=std::min(requested,pages-1);
    auto begin=page*limit;
    return {page,begin,begin+std::min(limit,count-begin),pages,page>0,page+1<pages};
}
}
