#include <acul/io/fs/jatc.hpp>
#include <acul/log.hpp>
#include <cassert>

void test_jatc()
{
    using namespace acul;
    using namespace acul::fs::jatc;

    task::thread_dispatch dispatch;

    const char *output_dir = getenv("TEST_OUTPUT_DIR");
    assert(output_dir);
    cache jatc(output_dir, dispatch);

    entrygroup group;
    group.name = "fonts";

    entrypoint *ep = jatc.register_entrypoint(&group);
    assert(ep != nullptr);

    response res;
    request req;
    req.group = &group;
    req.entrypoint = ep;
    req.write_callback = [](bin_stream &stream) { stream.write(42); };

    jatc.add_request(req, &res);

    jatc.await();
    ep->await();

    // Bin stream
    // Write
    const index_entry &original = res.entry();
    bin_stream stream;
    stream.write(original);

    // Read
    bin_stream read_stream;
    op_result state = jatc.read(ep, &group, original, read_stream);
    assert(state.success());

    read_stream.pos(0);
    int read_value = 0;
    read_stream.read(read_value);
    assert(read_value == 42);

    // === Check filter_index_entries(...) ===
    vector<index_entry *> index_entries{acul::alloc<index_entry>()};
    *index_entries[0] = original;

    jatc.filter_index_entries(ep, &group, index_entries);

    bin_stream filtered_stream;
    auto filtered_state = jatc.read(ep, &group, *index_entries[0], filtered_stream);
    assert(filtered_state.success());

    filtered_stream.pos(0);
    int filtered_value = 0;
    filtered_stream.read(filtered_value);
    assert(filtered_value == 42);

    // === Check index_entry  ===
    bin_stream index_test_stream;
    index_test_stream.write(original);

    index_test_stream.pos(0);
    index_entry deserialized{};
    index_test_stream.read(deserialized);

    assert(deserialized.offset == original.offset);
    assert(deserialized.size == original.size);
    assert(deserialized.checksum == original.checksum);
    assert(deserialized.compressed == original.compressed);

    assert(res.entrypoint == ep);
    assert(res.group == &group);

    jatc.deregister_entrypoint(ep, &group);
    dispatch.await();
}
