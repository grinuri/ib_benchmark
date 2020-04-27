#pragma once

#include <boost/mpi/datatype.hpp>
#include <boost/serialization/array.hpp>
#include <cereal/types/array.hpp>
#include <shmem.h>

namespace ib_bench {

/// a packet containing compile-time array, id, and rank
template<size_t Count, bool use_boost_serialization = true>
struct ct_ints {
    using value_type = unsigned int;
    size_t rank;
    int id;
    std::array<value_type, Count> data;

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
        return sizeof(rank) + sizeof(id) + data.size() * sizeof(value_type);
    }
};

///

/// a packet containing run-time array, id, and rank
template<bool=true, template <class> class Allocator = std::allocator>
struct rt_ints {
    using value_type = unsigned int;

    size_t rank;
    int id;
    std::vector<value_type, Allocator<value_type>> data;

    template <class Archive>
    void serialize(Archive& ar, unsigned int) {
        ar & rank;
        ar & id;
        ar & data;
    }

    size_t size() const {
        return sizeof(rank) + sizeof(id) + data.size() * sizeof(value_type);
    }
};

template <class T>
struct shmem_allocator {
    using value_type = T;

    shmem_allocator() = default;

    template <class U>
    constexpr shmem_allocator(const shmem_allocator<U>&) noexcept
    { }

    [[nodiscard]] T* allocate(std::size_t n) {
        if (n > std::size_t(-1) / sizeof(T)) {
            throw std::bad_alloc();
        }
        if (auto p = static_cast<T*>(shmem_malloc(n * sizeof(T)))) {
            return p;
        }
        throw std::bad_alloc();
    }

    void deallocate(T* p, std::size_t) noexcept {
        shmem_free(p);
    }
};

template <class T, class U>
bool operator==(const shmem_allocator<T>&, const shmem_allocator<U>&) {
    return true;
}
template <class T, class U>
bool operator!=(const shmem_allocator<T>&, const shmem_allocator<U>&) {
    return false;
}

struct shmem_rt_ints {
    using value_type = unsigned int;
    // [0]: rank
    // [1]: id
    std::vector<value_type, shmem_allocator<value_type>> data;

    size_t size() const {
        return data.size() * sizeof(value_type);
    }

    int rank() const {
        assert(data.size() > 2);
        return data[0];
    }

    int id() const {
        assert(data.size() > 2);
        return data[1];
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

template <>
struct generator<shmem_rt_ints> {
    using result_type = shmem_rt_ints;
    generator(size_t rank, size_t size) : m_rank(rank), m_id(0), m_size(size)
    { }
    result_type operator()() const {
        result_type result;
        prepare_data(result);
        std::generate(begin(result.data) + 2, end(result.data), std::rand);
        return result;
    }

    result_type operator()(unsigned int n) const {
        result_type result;
        prepare_data(result);
        std::fill(begin(result.data) + 2, end(result.data), n);
        return result;
    }

private:
    void prepare_data(result_type& result) const {
        result.data.resize(m_size + 2);
        result.data[0] = m_rank;
        result.data[1] = ++m_id;
    }

    size_t m_rank;
    mutable int m_id;
    size_t m_size;
};

// copy&paste
struct ucx_rt_ints {
    using value_type = unsigned int;
    // [0]: rank
    // [1]: id
    std::vector<value_type> data;

    size_t size() const {
        return data.size() * sizeof(value_type);
    }

    int rank() const {
        assert(data.size() > 2);
        return data[0];
    }

    int id() const {
        assert(data.size() > 2);
        return data[1];
    }
};

template <>
struct generator<ucx_rt_ints> {
    using result_type = ucx_rt_ints;
    generator(size_t rank, size_t size) : m_rank(rank), m_id(0), m_size(size)
    { }
    result_type operator()() const {
        result_type result;
        prepare_data(result);
        std::generate(begin(result.data) + 2, end(result.data), std::rand);
        return result;
    }

    result_type operator()(unsigned int n) const {
        result_type result;
        prepare_data(result);
        std::fill(begin(result.data) + 2, end(result.data), n);
        return result;
    }
    
    void set_meta(result_type& result) {
        result.data[0] = m_rank;
        result.data[1] = ++m_id;
    }

private:
    void prepare_data(result_type& result) const {
        result.data.resize(m_size + 2);
        result.data[0] = m_rank;
        result.data[1] = ++m_id;
    }

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
