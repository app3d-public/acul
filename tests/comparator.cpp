#include <acul/comparator.hpp>
#include <cassert>

void test_comparator()
{
    using namespace acul;

    case_insensitive_map<string, int> font_map;

    font_map.insert("Arial", {1, 2});
    assert(font_map.size() == 1);

    auto it = font_map["arial"];
    assert(it != font_map.end());
    assert(it->second.size() == 2);
    assert(it->second[0] == 1);
    assert(it->second[1] == 2);

    font_map.emplace("arial", 3);
    assert(font_map["ARIAL"]->second.size() == 3);
    assert(font_map["ARIAL"] != font_map.end());

    font_map.erase("ARIAL");
    assert(font_map.empty());

    font_map.insert("Roboto", {5});
    font_map.insert("Helvetica", {6});
    size_t count = 0;
    for (auto it = font_map.cbegin(); it != font_map.cend(); ++it) ++count;
    assert(count == font_map.size());
    font_map.clear();
    assert(font_map.empty());
    printf("test_case_insensitive_map passed.\n");
}
