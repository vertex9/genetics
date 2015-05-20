#ifndef BOOST_GENETICS_TWO_STAGE_INDEX_HPP
#define BOOST_GENETICS_TWO_STAGE_INDEX_HPP

#include <boost/genetics/augmented_string.hpp>

namespace boost { namespace genetics {
    /// two stage index, first index ordered by value, second by address.
    template <class StringType, class IndexArrayType, class AddrArrayType, bool Writable>
    class basic_two_stage_index {
    public:
        typedef typename IndexArrayType::value_type index_type;
        typedef typename AddrArrayType::value_type addr_type;

        basic_two_stage_index(
        ) : string(nullptr), num_indexed_chars(0) {
        }
        
        void write_binary(writer &wr) const {
            wr.write64(num_indexed_chars);
            wr.write(index);
            wr.write(addr);
        }

        basic_two_stage_index &operator =(basic_two_stage_index &&rhs) {
            string = rhs.string;
            index = std::move(rhs.index);
            addr = std::move(rhs.addr);
            return *this;
        }

        basic_two_stage_index(
            StringType &string,
            mapper &map
        ) :
            string(&string),
            num_indexed_chars((size_t)map.read64()),
            index(map),
            addr(map)
        {
            reindex();
        }
        
        basic_two_stage_index(
            StringType &string,
            size_t num_indexed_chars
        ) : string(&string), num_indexed_chars(num_indexed_chars) {
            reindex();
        }

        void reindex() {
            reindex_impl<Writable>();
        }

        class iterator {
        public:
            iterator(const basic_two_stage_index *tsi, const dna_string& str, size_t min_pos = 0, size_t max_distance = 0) :
                tsi(tsi), str(str), max_distance(max_distance)
            {
                size_t num_indexed_chars = tsi->num_indexed_chars;
                size_t num_seeds = str.size() / num_indexed_chars;

                if (num_seeds <= max_distance) {
                    pos = str.find_inexact(str, min_pos, max_distance);
                } else {
                    active.resize(num_seeds);
                    if (num_seeds == 0) {
                        pos = dna_string::npos;
                        return;
                    }

                    // get seed values from string
                    // touch the index in num_seeds places (with NTA hint)
                    for (size_t i = 0; i != num_seeds; ++i) {
                        active_state &s = active[i];
                        s.idx = (index_type)get_index(str, i * num_indexed_chars, num_indexed_chars);
                        touch_nta(tsi->addr.data() + tsi->index[i]);
                    }

                    // get seed values from string
                    // touch the address vector in num_seeds places (with stream hint)
                    for (size_t i = 0; i != num_seeds; ++i) {
                        active_state &s = active[i];
                        const addr_type *ptr = tsi->addr.data() + tsi->index[s.idx];
                        const addr_type *end = tsi->addr.data() + tsi->index[s.idx+1];
                        s.ptr = ptr;
                        s.end = end;
                        if (ptr != end) touch_stream(ptr);
                    }

                    for (size_t i = 0; i != num_seeds; ++i) {
                        active_state &s = active[i];
                        const addr_type *ptr = s.ptr;
                        const addr_type *end = s.end;
                        s.elem = (addr_type)i;
                        addr_type skip = (addr_type)(i * num_indexed_chars) + min_pos;
                        while (ptr != end && *ptr < skip) {
                            std::cout << "skip " << *ptr << "\n";
                            ++ptr;
                        }
                        s.start = ptr == end ? (addr_type)-1 : (addr_type)(*ptr - (s.elem * num_indexed_chars));
                        s.prev = (addr_type)-1;
                        s.ptr = ptr;
                    }

                    std::make_heap(active.begin(), active.end());
                    find_next();
                }
            }

            operator size_t() const {
                return pos;
            }

            iterator &operator++() {
                find_next();
                return *this;
            }

            iterator &operator++(int) {
                find_next();
                return *this;
            }
        private:
            void find_next() {
                size_t num_indexed_chars = tsi->num_indexed_chars;
                size_t num_seeds = str.size() / num_indexed_chars;

                if (pos == dna_string::npos) {
                    return;
                } else if (num_seeds <= max_distance) {
                    pos = str.find_inexact(str, pos + 1, max_distance);
                    printf("brute force\n");
                    return;
                } else {
                    addr_type prev_start = (addr_type)-1;
                    size_t repeat_count = 0;
                    pos = dna_string::npos;

                    for (;;) {
                        active_state s = active.front();

                        if (s.start != prev_start) {
                            size_t erc = 0;
                            // printf("xxx ");
                            // for (size_t i = 0; i != active.size(); ++i) {
                                // printf("%5d[%2d] ", (int)active[i].prev, (int)active[i].elem);
                                // erc += active[i].prev == prev_start;
                            // }
                            // printf("\n");
                            //printf("xxx rc=%d/%d\n", (int)repeat_count, (int)erc);
                            if (repeat_count >= num_seeds - max_distance) {
                                pos = prev_start;
                                return;
                            }
                            repeat_count = 0;
                            prev_start = s.start;
                        }

                        if (s.start == (addr_type)-1) {
                            return;
                        }

                        // printf("xxx %5d %2d/%2d [%2d]\n", (int)s.start, (int)repeat_count, (int)(num_seeds - max_distance), (int)s.elem);

                        const addr_type *ptr = s.ptr + 1;
                        const addr_type *end = s.end;
                        s.prev = s.start;
                        s.start = ptr == end ? (addr_type)-1 : (addr_type)(*ptr - s.elem * num_indexed_chars);
                        s.ptr = ptr;
                        std::pop_heap(active.begin(), active.end());
                        active.back() = s;
                        std::push_heap(active.begin(), active.end());
                        
                        // printf("xxx next: %d\n", (int)active.front().start);
                        repeat_count++;
                    }
                }
            }

            // search state
            struct active_state {
                const addr_type *ptr;
                const addr_type *end;
                index_type idx;
                addr_type start;
                addr_type prev;
                addr_type elem;

                bool operator<(const active_state &rhs) {
                    return start > rhs.start;
                }
            };

            const basic_two_stage_index *tsi;
            std::vector<active_state> active;
            const dna_string &str;
            size_t max_distance;
            size_t pos;
        };

        iterator find(const dna_string& str, size_t pos = 0, size_t max_distance = 0) const {
            return iterator(this, str, pos, max_distance);
        }

        template <class charT, class traits>
        void dump(std::basic_ostream<charT, traits>& os) const {
            typename std::basic_ostream<charT, traits>::fmtflags save = os.flags();
            size_t str_size = string->size();
            size_t index_size = (size_t)1 << (num_indexed_chars*2);
            for (size_t i = 0; i != index_size; ++i) {
                for (index_type j = index[i]; j != index[i+1]; ++j) {
                    addr_type a = addr[j];
                    os << "[" << a << " " << string->substr(a, num_indexed_chars) << "]";
                }
                os << "\n";
            }
            os.setstate( save );
        }
        
        void swap(basic_two_stage_index &rhs) {
            std::swap(string, rhs.string);
            std::swap(num_indexed_chars, rhs.num_indexed_chars);
            index.swap(rhs.index);
            addr.swap(rhs.addr);
        }
    private:
        // mapped version has no reindex() as we can't write to the vectors
        template<bool B>
        void reindex_impl() {
        }

        // std::vector version has reindex()
        template<>
        void reindex_impl<true>() {
            if (
                num_indexed_chars <= 1 ||
                num_indexed_chars > 32 ||
                string->size() < num_indexed_chars ||
                (addr_type)string->size() != string->size()
            ) {
                //throw std::out_of_range("reindex() failed");
            }

            size_t str_size = string->size();
            size_t index_size = (size_t)1 << (num_indexed_chars*2);

            index.resize(0);
            index.resize(index_size+1);
            addr.resize(str_size);

            size_t acc0 = 0;
            for (size_t i = 0; i != num_indexed_chars-1; ++i) {
                acc0 = acc0 * 4 + get_code(*string, i);
            }

            size_t acc = acc0;
            for (size_t i = num_indexed_chars; i != str_size; ++i) {
                acc = (acc * 4 + get_code(*string, i)) & (index_size-1);
                index[acc]++;
            }
            
            addr_type cur = 0;
            for (size_t i = 0; i != index_size; ++i) {
                addr_type val = index[i];
                index[i] = cur;
                cur += val;
            }
            index[index_size] = cur;

            acc = acc0;
            for (size_t i = num_indexed_chars; i != str_size; ++i) {
                acc = (acc * 4 + get_code(*string, i)) & (index_size-1);
                addr[index[acc]++] = (addr_type)(i - num_indexed_chars + 1);
            }

            addr_type prev = 0;
            for (size_t i = 0; i != index_size; ++i) {
                std::swap(prev, index[i]);
            }
        }

        // note: order matters
        StringType *string;
        size_t num_indexed_chars;
        IndexArrayType index;
        AddrArrayType addr;
    };

    typedef basic_two_stage_index<augmented_string, std::vector<uint32_t>, std::vector<uint32_t>, true > two_stage_index;
    typedef basic_two_stage_index<mapped_augmented_string, mapped_vector<uint32_t>, mapped_vector<uint32_t>, false > mapped_two_stage_index;

    template <class charT, class traits, class StringType, class IndexArrayType, class AddrArrayType, bool Writable>
    std::basic_ostream<charT, traits>&
    operator<<(std::basic_ostream<charT, traits>& os, const basic_two_stage_index<StringType, IndexArrayType, AddrArrayType, Writable>& x) {
        x.dump(os);
        return os;
    }
} }


#endif