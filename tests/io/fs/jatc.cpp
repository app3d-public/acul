#include <acul/io/fs/jatc.hpp>
#include <acul/io/fs/file.hpp>
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

    vector<char> large_data(20'000, 'A');
    response compressed_res;
    request compressed_req;
    compressed_req.group = &group;
    compressed_req.entrypoint = ep;
    compressed_req.write_callback = [large_data](bin_stream &stream) mutable {
        stream.write(large_data.data(), large_data.size());
    };
    jatc.add_request(compressed_req, &compressed_res);

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

    const index_entry compressed_original = compressed_res.entry();
    assert(compressed_original.compressed > 0);
    assert(compressed_original.size < large_data.size());

    bin_stream compressed_stream;
    state = jatc.read(ep, &group, compressed_original, compressed_stream);
    assert(state.success());
    assert(compressed_stream.size() == large_data.size());
    assert(std::equal(compressed_stream.begin(), compressed_stream.end(), large_data.begin()));

    // A restored entrypoint starts with an unknown in-memory position. Reopening
    // an existing file must validate its header without appending a new one.
    ep->fd.seekg(0, std::ios::end);
    const auto size_before_reopen = ep->fd.tellg();
    ep->fd.close();
    ep->pos = 0;

    bin_stream reopened_stream;
    state = jatc.read(ep, &group, original, reopened_stream);
    assert(state.success());
    ep->fd.seekg(0, std::ios::end);
    assert(ep->fd.tellg() == size_before_reopen);

    // === Check filter_index_entries(...) ===
    vector<index_entry *> index_entries{acul::alloc<index_entry>(), acul::alloc<index_entry>()};
    *index_entries[0] = original;
    *index_entries[1] = compressed_original;

    state = jatc.filter_index_entries(ep, &group, index_entries);
    assert(state.success());
    assert(index_entries[0]->compressed == 0);
    assert(index_entries[1]->compressed == 0);
    assert(index_entries[1]->size == large_data.size());

    // Successful persistence means the bytes are visible outside the still-live
    // entrypoint stream; the application keeps entrypoints alive until process exit.
    std::ifstream persisted(jatc.path(ep, &group).c_str(), std::ios::binary | std::ios::ate);
    assert(persisted.is_open());
    assert(persisted.tellg() ==
           static_cast<std::streamoff>(sizeof(header) + index_entries[0]->size + index_entries[1]->size));

    bin_stream filtered_stream;
    auto filtered_state = jatc.read(ep, &group, *index_entries[0], filtered_stream);
    assert(filtered_state.success());

    filtered_stream.pos(0);
    int filtered_value = 0;
    filtered_stream.read(filtered_value);
    assert(filtered_value == 42);

    bin_stream filtered_compressed_stream;
    filtered_state = jatc.read(ep, &group, *index_entries[1], filtered_compressed_stream);
    assert(filtered_state.success());
    assert(filtered_compressed_stream.size() == large_data.size());
    assert(std::equal(filtered_compressed_stream.begin(), filtered_compressed_stream.end(), large_data.begin()));

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

    // A group cleanup keeps its registered entrypoints and removes files whose
    // entrypoint IDs are no longer present in the group.
    entrypoint stale_ep{};
    stale_ep.id = ep->id + 1;
    if (stale_ep.id == 0) ++stale_ep.id;
    const auto stale_path = jatc.path(&stale_ep, &group);
    {
        std::ofstream stale_file(stale_path.c_str(), std::ios::binary | std::ios::trunc);
        assert(stale_file.is_open());
    }

    state = jatc.remove_unregistered_entrypoints(&group);
    assert(state.success());
    assert(fs::exists(jatc.path(ep, &group).c_str()));
    assert(!fs::exists(stale_path.c_str()));

    jatc.deregister_entrypoint(ep, &group);
    dispatch.await();
}
