#include <acul/comparator.hpp>
#include <cassert>

void test_comparator()
{
    using namespace acul;

    case_insensitive_map<string, int> fontMap;

    fontMap.insert("Arial", {1, 2});
    assert(fontMap.size() == 1);

    auto it = fontMap["arial"];
    assert(it != fontMap.end());
    assert(it->second.size() == 2);
    assert(it->second[0] == 1);
    assert(it->second[1] == 2);

    fontMap.emplace("arial", 3);
    assert(fontMap["ARIAL"]->second.size() == 3);
    assert(fontMap["ARIAL"] != fontMap.end());

    fontMap.erase("ARIAL");
    assert(fontMap.empty());

    fontMap.insert("Roboto", {5});
    fontMap.insert("Helvetica", {6});
    int count = 0;
    for (auto it = fontMap.cbegin(); it != fontMap.cend(); ++it) ++count;
    assert(count == fontMap.size());
    fontMap.clear();
    assert(fontMap.empty());
    printf("test_case_insensitive_map passed.\n");
}
