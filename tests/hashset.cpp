#include <acul/hash/hashset.hpp>
#include "hashset_common.hpp"

void test_hashset()
{
    using container_t = acul::hashset<int>;
    test_hashset_basic<container_t>();
    test_hashset_many_inserts_and_reads<container_t>();
    test_hashset_iteration<container_t>();
    test_hashset_idempotent_emplace<container_t>();
    test_hashset_erase<container_t>();
}
