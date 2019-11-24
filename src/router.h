#pragma once

#include <vector>
#include <unordered_map>

namespace ib_bench {

/**
 * Use: router router{
 *          comm_size, my_rank,
 *          {
 *              {0, {1, 2, 3}}, // 0 sends to 1, 2, 3 (in this order)
 *              {1, {0, 3}}
 *              {2, {3}},
 *              {4, {}} // 4 does not send
 *              // 3 is absent, so the default is applied
 *          },
 *          to_all // the default is "send to all", so 3 will send to all
 *      };
 *
 *      // currently each invocation returns the same route
 *      vector<router::rank_type> route = router();
 */
struct router {
    using rank_type = size_t;
    using route = std::vector<rank_type>;
    using routing_table = std::unordered_map<rank_type, route>;
    enum default_routing { to_all, to_none };

    router(
        rank_type comm_size,
        rank_type rank,
        routing_table routing_table = {},
        default_routing default_routing = to_all
    );
    route operator()() const;
    bool is_complete() const;

private:
    rank_type m_comm_size;
    rank_type m_rank;
    routing_table m_routing_table;
    default_routing m_default_routing;
};

/**
 * Table format:
 * 0: 1, 2, 3
 * 1: 0
 * 2:
 */
router::routing_table load_routing_table(const std::string& name);

}

