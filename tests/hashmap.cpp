#include <acul/hash/hashmap.hpp>
#include "hashmap_common.hpp"

void test_hashmap()
{
    using container_t = acul::hashmap<int, int>;
    test_hashmap_basic<container_t>();
    test_hashmap_many_inserts_and_reads<container_t>();
    test_hashmap_iteration<container_t>();
    test_hashmap_update_path<container_t>();
    test_hashmap_erase<container_t>();
}
