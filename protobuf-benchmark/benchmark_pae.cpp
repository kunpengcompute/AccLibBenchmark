#include <benchmark/benchmark.h>
#include <string>
#include <random>
#include <vector>
#include <cstring>
#include <iostream>
#include "pae_data.pb.h"

uint64_t genInt()
{
    std::string data;
    std::random_device rd;
    std::mt19937 engine(rd());
    std::uniform_int_distribution<uint64_t> dist(1, std::numeric_limits<uint32_t>::max());
    //std::uniform_int_distribution<uint64_t> dist(1, 16777215);
    //std::uniform_int_distribution<uint64_t> dist(1, 255);
    return dist(engine);
    //return 655356;
}

idl::lagrange::common::InferenceMeta genData(size_t arrSize)
{
    idl::lagrange::common::InferenceMeta interface;
    //对最外层赋值
    interface.set_batch_size(genInt());
    idl::lagrange::common::GPUTransLayerMeta* firstInner = interface.add_emb_meta();
    //对中间层赋值
    firstInner->set_name("ABC");
    firstInner->set_emb_name("ABCD");
    firstInner->set_data_size(10000);
    firstInner->set_shared(true);
    firstInner->set_empty(false);
    firstInner->set_tiling_in_graph(true);
    idl::lagrange::common::GPUTransBiasMeta* secondInner1 = firstInner->mutable_bias();
    idl::lagrange::common::GPUTransVecMeta* secondInner2 = firstInner->mutable_vec();
    idl::lagrange::common::GPUTrans3DMeta* secondInner3 = firstInner->mutable_seq();
    //对最内层赋值
    secondInner1->add_fc_name("ABCE");
    secondInner2->add_fc_name("ABCF");
    secondInner3->add_fc_name("ABCG");
    for (size_t i = 1; i <= arrSize; ++i) {
        uint64_t n = genInt();
        //secondInner1->add_output_id(n);
        //n = genInt();
        //secondInner2->add_meta_dim(n);
        //n = genInt();
        //secondInner2->add_meta_size(n);
        //n = genInt();
        //secondInner2->add_input_offset(n);
        //n = genInt();
        //secondInner2->add_output_offset(n);
        //n = genInt();
        secondInner3->add_input_offset(n);
        n = genInt();
        secondInner3->add_output_id(n);
        n = genInt();
        secondInner3->add_feature_size(n);
    }
    return interface;
}

std::string res;

#define DEFINE_BENCHMARK(size)                                      \
    idl::lagrange::common::InferenceMeta Data##size = genData(size);\
    static void BM_ser_##size(benchmark::State &state)              \
    {                                                               \
        for (auto _ : state) {                                      \
            res = Data##size.SerializeAsString();                   \
        }                                                           \
    }                                                               \
    BENCHMARK(BM_ser_##size);

#define DEFINE_DESERIALIZE_BENCHMARK(size)                            \
    idl::lagrange::common::InferenceMeta DataDe##size = genData(size);\
    idl::lagrange::common::InferenceMeta DataDe2##size;               \
    std::string res##size = DataDe##size.SerializeAsString();         \
    static void BM_Deser_##size(benchmark::State &state)              \
    {                                                                 \
        for (auto _ : state) {                                        \
            DataDe2##size.ParseFromString(res##size);                 \
        }                                                             \
    }                                                                 \
    BENCHMARK(BM_Deser_##size);


DEFINE_BENCHMARK(10)
DEFINE_BENCHMARK(15)
DEFINE_BENCHMARK(20)
DEFINE_BENCHMARK(25)
DEFINE_BENCHMARK(40)
DEFINE_BENCHMARK(100)
DEFINE_BENCHMARK(500)
DEFINE_BENCHMARK(1000)
DEFINE_BENCHMARK(2000)
DEFINE_BENCHMARK(4000)

DEFINE_DESERIALIZE_BENCHMARK(10)
DEFINE_DESERIALIZE_BENCHMARK(15)
DEFINE_DESERIALIZE_BENCHMARK(20)
DEFINE_DESERIALIZE_BENCHMARK(25)
DEFINE_DESERIALIZE_BENCHMARK(40)
DEFINE_DESERIALIZE_BENCHMARK(100)
DEFINE_DESERIALIZE_BENCHMARK(500)
DEFINE_DESERIALIZE_BENCHMARK(1000)
DEFINE_DESERIALIZE_BENCHMARK(2000)
DEFINE_DESERIALIZE_BENCHMARK(4000)
BENCHMARK_MAIN();
