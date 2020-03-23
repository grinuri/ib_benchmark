#include "router.h"

#include <vector>
#include <unordered_set>

#include <fstream>
#include <boost/spirit/home/x3.hpp>
#include <boost/fusion/adapted/std_pair.hpp>

namespace ib_bench {

router::router(
    rank_type comm_size,
    rank_type rank,
    routing_table routing_table,
    default_routing default_routing
) :
    m_comm_size(comm_size),
    m_rank(rank),
    m_routing_table(std::move(routing_table)),
    m_default_routing(default_routing)
{}

router::route router::operator()() const {
    route result;

    if (
        auto pos = m_routing_table.find(m_rank);
        pos == m_routing_table.end() && m_default_routing == to_all
    ) {
        result.reserve(m_comm_size - 1);
        for (rank_type dest = (m_rank + 1) % m_comm_size; dest < m_rank; dest = (dest + 1) % m_comm_size) {
            if (dest != m_rank) {
                result.push_back(dest);
            }
        }
    } else {
        result = pos->second;
        result.erase(
            std::remove_if(
                begin(result),
                end(result),
                [this](rank_type rank) { return rank >= m_comm_size; }
            ),
            result.end()
        );
    }
    return result;
}

bool router::is_complete() const {
    // some sources are missing?
    if (m_default_routing != to_all && m_routing_table.size() < m_comm_size) {
        return false;
    }
    for (const auto& [from, tos] : m_routing_table) {
        std::unordered_set<rank_type> route;
        route.insert(from);
        for (auto to : tos) {
            route.insert(to);
        }
        // some destination are missing?
        if (route.size() < m_comm_size) {
            return false;
        }
    }
    return true;
}

router::routing_table load_routing_table(const std::string& name) {
    router::routing_table result;
    // no file, no table - it's ok
    std::ifstream f(name);
    std::string line;
    while (getline(f, line)) {
        std::pair<size_t, std::vector<size_t>> out;
        using namespace boost::spirit::x3;
        phrase_parse(line.begin(), line.end(), uint_ >> ':' >> *(uint_ % ','), blank, out);
        result.insert(std::move(out));
    }
    return result;
}

}

