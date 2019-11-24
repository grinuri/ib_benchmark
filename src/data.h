#pragma once

#include <boost/mpi/datatype.hpp>
#include <boost/serialization/array.hpp>
#include <cereal/types/array.hpp>
#include <boost/serialization/vector.hpp>

namespace ib_bench {

/// a packet containing compile-time array, id, and rank
template<size_t Count, bool use_boost_serialization = true>
struct ct_ints {
    size_t rank;
    int id;
    std::array<unsigned int, Count> data;

    template <class Archive, bool boost = use_boost_serialization>
    std::enable_if_t<!boost> serialize(Archive& ar) {
        ar(rank);
        ar(id);
        ar(data);
    }

    template <class Archive, bool boost = use_boost_serialization>
    std::enable_if_t<boost> serialize(Archive& ar, unsigned int) {
        ar & rank;
        ar & id;
        ar & data;
    }

    constexpr size_t size() const {
        return sizeof(rank) + sizeof(id) + sizeof(data);
    }
};

///

/// a packet containing run-time array, id, and rank
template<bool=true>
struct rt_ints {
    size_t rank;
    int id;
    std::vector<unsigned int> data;

    template <class Archive>
    void serialize(Archive& ar, unsigned int) {
        ar & rank;
        ar & id;
        ar & data;
    }

    size_t size() const {
        return sizeof(rank) + sizeof(id) + data.size();
    }
};


template <class T>
struct generator {
    T operator()() const {
        return rand();
    }
    T operator()(unsigned int n) const {
        return n;
    }
};

/// Fills data with either random ints or a user-defined value
template <size_t Count, bool use_boost_serialization>
struct generator<ct_ints<Count, use_boost_serialization>> {
    using result_type = ct_ints<Count, use_boost_serialization>;
    generator(size_t rank) : m_rank(rank), m_id(0)
    { }
    result_type operator()() const {
        result_type result{m_rank, ++m_id, {}};
        std::generate(begin(result.data), end(result.data), std::rand);
        return result;
    }

    result_type operator()(unsigned int n) const {
        result_type result{m_rank, ++m_id, {}};
        std::fill(begin(result.data), end(result.data), n);
        return result;
    }

private:
    size_t m_rank;
    mutable int m_id;
};

/// Fills data with either random ints or a user-defined value
template <bool use_boost_serialization>
struct generator<rt_ints<use_boost_serialization>> {
    using result_type = rt_ints<use_boost_serialization>;
    generator(size_t rank, size_t size) : m_rank(rank), m_id(0), m_size(size)
    { }
    result_type operator()() const {
        result_type result{m_rank, ++m_id, {}};
        result.data.resize(m_size);
        std::generate(begin(result.data), end(result.data), std::rand);
        return result;
    }

    result_type operator()(unsigned int n) const {
        result_type result{m_rank, ++m_id, {}};
        result.data.resize(m_size);
        std::fill(begin(result.data), end(result.data), n);
        return result;
    }

private:
    size_t m_rank;
    mutable int m_id;
    size_t m_size;
};

}

namespace boost::mpi {
    template <size_t Count, bool use_boost_serialization>
    struct is_mpi_datatype<ib_bench::ct_ints<Count, use_boost_serialization>> : mpl::true_ {
    };
}
