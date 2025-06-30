#include <acul/memory.hpp>
#include <acul/meta.hpp>
#include <acul/stream.hpp>
#include <acul/vector.hpp>
#include <cassert>
#include <cstring>

void test_meta()
{
    using namespace acul;
    using namespace acul::meta;

    task::service_dispatch sd;
    sd.run();
    auto *service = acul::alloc<log::log_service>();
    sd.register_service(service);
    service->level = log::level::Trace;

    auto *console = service->add_logger<log::console_logger>("console");
    service->default_logger = console;
    console->set_pattern("%(message)\n");
    assert(console->name() == "console");

    hash_resolver meta_resolver;
    meta_resolver.streams = {
        {sign_block::Raw, &streams::raw_block},
    };
    meta::resolver = &meta_resolver;

    const char testData[] = "hello_meta";
    u64 testSize = sizeof(testData);

    char *buffer = acul::alloc_n<char>(testSize);
    memcpy(buffer, testData, testSize);

    shared_ptr<block> original = make_shared<raw_block>(buffer, testSize);

    bin_stream out;
    auto *stream = meta::resolver->get_stream(original->signature());
    assert(stream);
    stream->write(out, original.get());

    bin_stream in(out);
    shared_ptr<block> inMeta{stream->read(in)};
    assert(inMeta);
    shared_ptr<raw_block> loaded = static_pointer_cast<raw_block>(inMeta);

    assert(loaded->signature() == sign_block::Raw);
    assert(loaded->data_size == testSize);
    assert(memcmp(loaded->data, testData, testSize) == 0);
}
