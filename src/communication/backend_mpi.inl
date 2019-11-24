#include <boost/format.hpp>
#include <util/log.h>
#include <util/type_name.h>

namespace ib_bench {

template <class FrontEnd>
void MPIBackend::validate_frontend_type() {
    BENCH_LOG_DEBUG(
        boost::format("[%d] Validating comm frontend type") % rank());

    m_world.barrier();
    std::string front_end_type_name = type_name<FrontEnd>();
    auto reduce = [](const std::string& name1, const std::string& name2) {
        return name1 == name2 ? name1 : std::string();
    };

    std::string out;
    bmpi::reduce(m_world, front_end_type_name, out, reduce, 0);
    if (rank() == 0 && out.empty()) {
        throw std::runtime_error("Fatal error! Communicator types of "
                                 "different nodes do not match!");
    }
    if (rank() == 0) {
        BENCH_LOG_DEBUG("[0] Comm type validation successful");
    }
}

}
