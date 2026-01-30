# App3D Core Utils Library

**Acul** is the foundational runtime library of the App3D project.
It provides a set of utilities, core subsystems, and selective replacements for areas where standard solutions are insufficient for the performance, integration, or control requirements of the App3D ecosystem.
It serves as the runtime backbone for App3D modules and tools.

The library does not attempt to reimplement the standard library wholesale.
Only components that exhibit practical limitations in real-world usage are reimplemented.
Everything else is expected to be used as-is from the surrounding ecosystem.

## Major Components

### Strings
- Custom string type based on the SSO-23 model (small-string optimization).  
- One bit of the size field is reserved to mark `.rodata` mode:  
  in this mode the string directly references string literals from the read-only segment instead of copying them to stack or heap.  
- Utilities are provided for working with strings (views, pools, in-memory string streams).

> [!WARNING]
> Construction directly from `char[]` temporarily buffers is undefined behavior – use `(const char*)buf` to force a heap/SSO copy.

### Exceptions
- Exception system with captured stack trace on throw.

> [!NOTE]
> On Microsoft Windows, you must call `destroy_exception_context()` at program shutdown for compatibility with remote memory access.  
> On Linux, remote stack capture requires enabling `ptrace` API to allow tracing the process.

### Hash Maps / Sets
- Hash containers in `acul` are designed with a focus on fast `emplace` operations and efficient miss lookups.
- Two families are provided:
  - `acul::hashmap` / `acul::hashset`: chain-based implementation, well-suited for small tables.  
  - `acul::hl_hashmap` / `acul::hl_hashset`: an open-addressed hash table optimized for large datasets, featuring ISA-specific hardware acceleration

### Smart Pointers
- Custom `shared_ptr`, `weak_ptr`, and `unique_ptr`.  
- Optimized for single-threaded performance.

> [!WARNING]
> Acul smart pointers are not thread-safe.

### Buffer Containers
- Custom `vector`, `list`, `forward_list`  
- Designed for integration with App3D’s memory model.

> [!NOTE]
> Acul containers are not compatible with std allocators.

### Memory
- Memory utilities and allocation adapters.

### Concurrency & Utilities
- Task management subsystem.
- Task sheduler subsystem.
- Logging subsystem.
- Deferred destruction queue.
- Atomic/Futex based synchronization `shared_mutex` implementation.
- Locale-related helpers.

### IO
- Basic file utilities for cross-platform file operations.  

#### Paths
- Unified path abstraction for both local and virtual schemes.  
- Internally all paths are stored as UTF-8, independent of host OS encoding.

#### JATC (Journalable Asynchronous Temporary Cache)
JATC is an asynchronous caching subsystem built around a journaled file format.  
Its purpose is to provide safe, concurrent access to temporary data with guaranteed integrity.  

**Design overview:**\
Data is organized into **entrygroups**, which register one or more **entrypoints**.  
Each entrypoint represents a journal file and manages synchronization for concurrent operations.  
Client code interacts with JATC through request/response pairs.  
Each response resolves to an **index entry**, which contains the metadata locating data inside the entrypoint journal.

## Building

### Supported compilers:
- GNU GCC
- Clang

### Supported OS:
- Linux
- Microsoft Windows


### C++ Requirements:
- C++17 or newer
- CXX GNU Extensions

### Cmake options:
- `ACUL_INTL_ENABLE`: Enable intl support
- `ACUL_ZSTD_ENABLE`: Enable zstd support
- `BUILD_TESTS`: Enable testing
- `ENABLE_COVERAGE`: Enable code coverage

### External packages
These are system libraries that must be available at build time:

- [OneAPI TBB](https://github.com/oneapi-src/oneTBB) — required
- [zstd](https://github.com/facebook/zstd) — optional, enables compressed buffers and caching in JATC
- [gettext (intl)](https://www.gnu.org/software/gettext/) — optional, enables locale-aware messages and internationalization

### Bundled submodules
The following dependencies are included as git submodules and must be checked out when cloning:

- [acbt](https://github.com/app3d-public/acbt) - App3D Cmake Build Tools

## Benchmarks
<details>
<summary>Hashmap</summary>

<details>
<summary>Insert</summary>

## Insert — uint64_t (1000)

| Hashmap             | bw_gib_s | cps_avg  | cps_max  | cps_min  |
|---------------------|----------|----------|----------|----------|
| acul::hashmap       | 2.94646  | 263.644M | 277.666M | 248.906M |
| acul::hl_hashmap    | 1.92429  | 172.183M | 175.886M | 166.080M |
| emhash5             | 1.33734  | 119.663M | 123.837M | 114.488M |
| emhash7             | 1.54249  | 138.020M | 141.014M | 135.634M |
| absl::flat_hash_map | 1.07026  | 95.7653M | 99.1258M | 91.5078M |
| llvm::DenseMap      | 1.31555  | 117.713M | 121.598M | 110.421M |
| std::unordered_map  | 0.43690  | 39.0931M | 40.0286M | 37.8366M |


## Insert — uint64_t (1,000,000)

| Hashmap             | bw_gib_s | cps_avg  | cps_max  | cps_min  |
|---------------------|----------|----------|----------|----------|
| acul::hashmap       | 0.343693 | 30.7532M | 32.4665M | 28.9412M |
| acul::hl_hashmap    | 0.576819 | 51.6128M | 53.241M  | 47.7161M |
| emhash5             | 0.339040 | 30.3368M | 31.0223M | 29.4363M |
| emhash7             | 0.405489 | 36.2826M | 38.2325M | 34.0677M |
| absl::flat_hash_map | 0.452130 | 40.4559M | 41.2923M | 39.3651M |
| llvm::DenseMap      | 0.242297 | 21.6804M | 22.0758M | 21.3965M |
| std::unordered_map  | 0.047743 | 4.272M   | 4.45734M | 4.11847M |

---

## Insert — Vec3f (1000)

| Hashmap             | bw_gib_s | cps_avg  | cps_max  | cps_min  |
|---------------------|----------|----------|----------|----------|
| acul::hashmap       | 0.195229 | 13.1016M | 13.4878M | 11.990M  |
| acul::hl_hashmap    | 1.79292  | 120.321M | 124.084M | 112.476M |
| emhash5             | 0.082887 | 5.56246M | 5.83773M | 5.25495M |
| emhash7             | 0.093074 | 6.24614M | 6.411M   | 6.01508M |
| absl::flat_hash_map | 0.939135 | 63.0243M | 64.7377M | 60.5535M |
| llvm::DenseMap      | 0.035392 | 2.3751M  | 2.44986M | 2.31168M |
| std::unordered_map  | 0.417821 | 28.0395M | 28.9776M | 26.7587M |

## Insert — Vec3f (1,000,000)

| Hashmap             | bw_gib_s | cps_avg  | cps_max  | cps_min  |
|---------------------|----------|----------|----------|----------|
| acul::hashmap       | 0.196161 | 13.1642M | 13.4137M | 12.8226M |
| acul::hl_hashmap    | 0.587755 | 39.4436M | 41.6542M | 38.0788M |
| emhash5             | 0.187300 | 12.5695M | 12.8546M | 11.9167M |
| emhash7             | 0.228832 | 15.3566M | 16.0243M | 14.4343M |
| absl::flat_hash_map | 0.483233 | 32.4292M | 33.7741M | 31.1822M |
| llvm::DenseMap      | 0.152944 | 10.2639M | 10.4822M | 9.48688M |
| std::unordered_map  | 0.042344 | 2.84163M | 2.85236M | 2.83098M |

---

## Insert — string (1000)

| Hashmap             | bw_gib_s | cps_avg  | cps_max  | cps_min  |
|---------------------|----------|----------|----------|----------|
| acul::hashmap       | 0.838758 | 45.0305M | 46.6957M | 43.7994M |
| acul::hl_hashmap    | 1.00839  | 54.1374M | 55.0377M | 52.2687M |
| emhash5             | 0.648140 | 34.7968M | 36.0312M | 32.6773M |
| emhash7             | 0.739815 | 39.7185M | 41.6289M | 37.6662M |
| absl::flat_hash_map | 0.763108 | 40.969M  | 43.2287M | 38.4338M |
| llvm::DenseMap      | 0.365495 | 19.6224M | 20.5715M | 18.6592M |
| std::unordered_map  | 0.440757 | 23.663M  | 24.7574M | 22.0749M |

## Insert — string (1,000,000)

| Hashmap             | bw_gib_s | cps_avg  | cps_max  | cps_min  |
|---------------------|----------|----------|----------|----------|
| acul::hashmap       | 0.143539 | 7.7062M  | 7.98828M | 7.50737M |
| acul::hl_hashmap    | 0.315190 | 16.9216M | 17.6727M | 16.4135M |
| emhash5             | 0.149963 | 8.05109M | 8.17995M | 7.94236M |
| emhash7             | 0.171496 | 9.20712M | 9.51238M | 8.79068M |
| absl::flat_hash_map | 0.276972 | 14.8698M | 14.9895M | 14.6368M |
| llvm::DenseMap      | 0.117099 | 6.28668M | 6.53355M | 6.00563M |
| std::unordered_map  | 0.040382 | 2.16802M | 2.24583M | 2.09541M |

---
</details>
<details>
<summary>Find</summary>

## Find — uint64_t (1000, hit)

| Hashmap             | bw_gib_s | cps_avg  | cps_max  | cps_min  |
|---------------------|----------|----------|----------|----------|
| acul::hashmap       | 12.5911  | 1.68995G | 1.71221G | 1.65502G |
| acul::hl_hashmap    | 6.62438  | 889.109M | 901.401M | 863.737M |
| emhash5             | 11.1025  | 1.49015G | 1.52325G | 1.45851G |
| emhash7             | 9.17184  | 1.23102G | 1.25339G | 1.21546G |
| absl::flat_hash_map | 4.83677  | 649.180M | 652.459M | 645.784M |
| llvm::DenseMap      | 9.18477  | 1.23276G | 1.24920G | 1.20536G |
| std::unordered_map  | 7.53655  | 1.01154G | 1.02939G | 989.658M |

## Find — uint64_t (1000, miss)

| Hashmap             | bw_gib_s | cps_avg  | cps_max  | cps_min  |
|---------------------|----------|----------|----------|----------|
| acul::hashmap       | 10.9216  | 1.46588G | 1.50236G | 1.42471G |
| acul::hl_hashmap    | 3.48016  | 467.099M | 474.403M | 460.015M |
| emhash5             | 10.6083  | 1.42383G | 1.45657G | 1.39606G |
| emhash7             | 8.11686  | 1.08943G | 1.10968G | 1.05610G |
| absl::flat_hash_map | 5.41310  | 726.535M | 744.020M | 715.372M |
| llvm::DenseMap      | 4.06431  | 545.502M | 559.407M | 534.731M |
| std::unordered_map  | 2.85736  | 383.508M | 389.855M | 373.857M |

## Find — uint64_t (1,000,000, hit)

| Hashmap             | bw_gib_s | cps_avg  | cps_max  | cps_min  |
|---------------------|----------|----------|----------|----------|
| acul::hashmap       | 1.00087  | 134.334M | 142.424M | 123.551M |
| acul::hl_hashmap    | 0.644642 | 86.5224M | 91.2183M | 81.8194M |
| emhash5             | 1.27921  | 171.693M | 179.076M | 158.972M |
| emhash7             | 1.17759  | 158.054M | 168.788M | 149.946M |
| absl::flat_hash_map | 0.565126 | 75.850M  | 76.9681M | 74.3296M |
| llvm::DenseMap      | 0.576483 | 77.3743M | 82.2843M | 74.4492M |
| std::unordered_map  | 0.364425 | 48.9122M | 54.4363M | 43.926M  |

## Find — uint64_t (1,000,000, miss)

| Hashmap             | bw_gib_s | cps_avg  | cps_max  | cps_min  |
|---------------------|----------|----------|----------|----------|
| acul::hashmap       | 0.945105 | 126.850M | 135.264M | 115.908M |
| acul::hl_hashmap    | 2.02735  | 272.107M | 279.761M | 258.103M |
| emhash5             | 1.17645  | 157.900M | 174.292M | 149.044M |
| emhash7             | 1.02735  | 137.888M | 145.344M | 129.883M |
| absl::flat_hash_map | 1.24731  | 167.411M | 172.084M | 160.212M |
| llvm::DenseMap      | 0.349273 | 46.8786M | 48.2111M | 45.1401M |
| std::unordered_map  | 0.194280 | 26.0758M | 27.0187M | 24.0109M |

---

## Find — Vec3f (1000, hit)

| Hashmap             | bw_gib_s | cps_avg  | cps_max  | cps_min  |
|---------------------|----------|----------|----------|----------|
| acul::hashmap       | 0.171985 | 15.389M  | 15.9047M | 14.9843M |
| acul::hl_hashmap    | 1.95442  | 174.878M | 179.957M | 167.683M |
| emhash5             | 0.106111 | 9.49466M | 9.98088M | 9.20181M |
| emhash7             | 0.128196 | 11.4707M | 11.7479M | 11.214M  |
| absl::flat_hash_map | 2.25025  | 201.349M | 204.363M | 198.589M |
| llvm::DenseMap      | 0.226538 | 20.2703M | 20.46M   | 19.9775M |
| std::unordered_map  | 1.89947  | 169.961M | 172.542M | 166.020M |

## Find — Vec3f (1000, miss)

| Hashmap             | bw_gib_s | cps_avg  | cps_max  | cps_min  |
|---------------------|----------|----------|----------|----------|
| acul::hashmap       | 0.090040 | 8.05665M | 8.27446M | 7.73294M |
| acul::hl_hashmap    | 2.31809  | 207.420M | 212.883M | 202.126M |
| emhash5             | 0.057740 | 5.16649M | 5.33166M | 4.98813M |
| emhash7             | 0.068186 | 6.10114M | 6.27717M | 5.91675M |
| absl::flat_hash_map | 2.68678  | 240.409M | 246.244M | 235.382M |
| llvm::DenseMap      | 0.113799 | 10.1826M | 10.2435M | 9.97735M |
| std::unordered_map  | 1.95935  | 175.320M | 177.842M | 171.410M |

## Find — Vec3f (1,000,000, hit)

| Hashmap             | bw_gib_s | cps_avg  | cps_max  | cps_min  |
|---------------------|----------|----------|----------|----------|
| acul::hashmap       | 0.387484 | 34.6715M | 36.343M  | 33.3249M |
| acul::hl_hashmap    | 0.374499 | 33.5096M | 35.3144M | 31.6508M |
| emhash5             | 0.414750 | 37.1112M | 38.4369M | 35.764M  |
| emhash7             | 0.402130 | 35.982M  | 38.3155M | 33.6129M |
| absl::flat_hash_map | 0.410638 | 36.7433M | 37.7827M | 35.0378M |
| llvm::DenseMap      | 0.331850 | 29.6935M | 30.6466M | 27.6626M |
| std::unordered_map  | 0.147668 | 13.2131M | 13.6101M | 12.9506M |

## Find — Vec3f (1,000,000, miss)

| Hashmap             | bw_gib_s | cps_avg  | cps_max  | cps_min  |
|---------------------|----------|----------|----------|----------|
| acul::hashmap       | 0.430985 | 38.5639M | 40.5238M | 36.360M  |
| acul::hl_hashmap    | 1.52771  | 136.698M | 145.250M | 128.128M |
| emhash5             | 0.375224 | 33.5745M | 35.1203M | 32.158M  |
| emhash7             | 0.416812 | 37.2957M | 38.299M  | 36.2183M |
| absl::flat_hash_map | 1.04106  | 93.1529M | 96.9265M | 87.9636M |
| llvm::DenseMap      | 0.232838 | 20.834M  | 21.7135M | 19.4907M |
| std::unordered_map  | 0.100518 | 8.99421M | 9.50735M | 8.33036M |

---

## Find — string (1000, hit)

| Hashmap             | bw_gib_s | cps_avg  | cps_max  | cps_min  |
|---------------------|----------|----------|----------|----------|
| acul::hashmap       | 1.13576  | 76.2193M | 77.8665M | 72.3386M |
| acul::hl_hashmap    | 0.847846 | 56.898M  | 66.8375M | 36.719M  |
| emhash5             | 0.960348 | 64.4479M | 83.0579M | 46.8189M |
| emhash7             | 0.953039 | 63.9574M | 70.405M  | 56.4983M |
| absl::flat_hash_map | 1.09509  | 73.490M  | 75.397M  | 71.3716M |
| llvm::DenseMap      | 1.01780  | 68.3033M | 69.7565M | 66.6861M |
| std::unordered_map  | 0.866838 | 58.1725M | 59.218M  | 57.4211M |

## Find — string (1000, miss)

| Hashmap             | bw_gib_s | cps_avg  | cps_max  | cps_min  |
|---------------------|----------|----------|----------|----------|
| acul::hashmap       | 2.27574  | 152.723M | 157.386M | 142.096M |
| acul::hl_hashmap    | 1.80512  | 121.140M | 142.458M | 109.037M |
| emhash5             | 1.62830  | 109.273M | 120.377M | 92.986M  |
| emhash7             | 2.29587  | 154.073M | 168.833M | 121.402M |
| absl::flat_hash_map | 2.42610  | 162.813M | 166.633M | 157.983M |
| llvm::DenseMap      | 1.01854  | 68.3534M | 70.053M  | 66.4477M |
| std::unordered_map  | 1.55302  | 104.222M | 109.227M | 100.026M |

## Find — string (1,000,000, hit)

| Hashmap             | bw_gib_s | cps_avg  | cps_max  | cps_min  |
|---------------------|----------|----------|----------|----------|
| acul::hashmap       | 0.182832 | 12.2696M | 12.4732M | 12.023M  |
| acul::hl_hashmap    | 0.203311 | 13.644M  | 13.9769M | 13.2483M |
| emhash5             | 0.198450 | 13.3178M | 14.1071M | 12.1108M |
| emhash7             | 0.189894 | 12.7436M | 13.2491M | 12.3515M |
| absl::flat_hash_map | 0.186696 | 12.529M  | 13.0208M | 12.2448M |
| llvm::DenseMap      | 0.216106 | 14.5026M | 15.0082M | 13.9342M |
| std::unordered_map  | 0.080572 | 5.40713M | 5.75022M | 4.96095M |

## Find — string (1,000,000, miss)

| Hashmap             | bw_gib_s | cps_avg  | cps_max  | cps_min  |
|---------------------|----------|----------|----------|----------|
| acul::hashmap       | 0.314863 | 21.1301M | 22.8659M | 16.5495M |
| acul::hl_hashmap    | 1.10761  | 74.3304M | 88.8837M | 67.1685M |
| emhash5             | 0.255933 | 17.1754M | 18.1494M | 16.1298M |
| emhash7             | 0.401819 | 26.9656M | 28.6228M | 24.9891M |
| absl::flat_hash_map | 0.780933 | 52.4075M | 57.142M  | 47.2008M |
| llvm::DenseMap      | 0.177993 | 11.9449M | 12.3187M | 11.3449M |
| std::unordered_map  | 0.111028 | 7.45098M | 7.89688M | 7.17646M |

</details>

<details>
<summary>Erase</summary>

## Erase Half — uint64_t (1000)

| Hashmap             | bw_gib_s | cps_avg  | cps_max  | cps_min  |
|---------------------|----------|----------|----------|----------|
| acul::hashmap       | 8.93403  | 1.19911G | 1.21570G | 1.17875G |
| acul::hl_hashmap    | 3.53921  | 475.025M | 478.018M | 469.827M |
| emhash5             | 7.59986  | 1.02004G | 1.03859G | 1.00666G |
| emhash7             | 5.71407  | 766.929M | 786.488M | 727.032M |
| absl::flat_hash_map | 1.68743  | 226.483M | 234.158M | 211.653M |
| llvm::DenseMap      | 4.93010  | 661.707M | 667.039M | 646.517M |
| std::unordered_map  | 0.565683 | 75.9247M | 77.3508M | 73.8087M |

## Erase Half — uint64_t (1,000,000)

| Hashmap             | bw_gib_s | cps_avg  | cps_max  | cps_min  |
|---------------------|----------|----------|----------|----------|
| acul::hashmap       | 2.32624  | 312.223M | 318.967M | 305.259M |
| acul::hl_hashmap    | 0.444151 | 59.613M  | 62.3607M | 57.2212M |
| emhash5             | 2.89555  | 388.634M | 407.696M | 348.314M |
| emhash7             | 2.63583  | 353.776M | 372.532M | 311.012M |
| absl::flat_hash_map | 0.256263 | 34.3951M | 37.2500M | 32.5980M |
| llvm::DenseMap      | 0.561720 | 75.3927M | 77.8390M | 72.9069M |
| std::unordered_map  | 0.425421 | 57.099M  | 59.0301M | 53.5284M |

## Insert After Erase — uint64_t (1000)

| Hashmap             | bw_gib_s | cps_avg  | cps_max  | cps_min  |
|---------------------|----------|----------|----------|----------|
| acul::hashmap       | 3.36425  | 301.028M | 304.972M | 297.094M |
| acul::hl_hashmap    | 2.11484  | 189.232M | 193.444M | 185.468M |
| emhash5             | 4.48939  | 401.703M | 408.007M | 395.438M |
| emhash7             | 3.47400  | 310.848M | 317.507M | 297.442M |
| absl::flat_hash_map | 3.13967  | 280.933M | 284.706M | 274.646M |
| llvm::DenseMap      | 3.14694  | 281.583M | 283.706M | 279.833M |
| std::unordered_map  | 0.488841 | 43.7408M | 44.2249M | 43.0573M |

## Insert After Erase — uint64_t (1,000,000)

| Hashmap             | bw_gib_s | cps_avg  | cps_max  | cps_min  |
|---------------------|----------|----------|----------|----------|
| acul::hashmap       | 0.490503 | 43.8895M | 45.5478M | 41.8909M |
| acul::hl_hashmap    | 0.728408 | 65.1769M | 67.9737M | 63.4141M |
| emhash5             | 0.445862 | 39.8951M | 43.0712M | 33.7360M |
| emhash7             | 0.753985 | 67.4654M | 71.3366M | 59.8050M |
| absl::flat_hash_map | 0.672781 | 60.1994M | 65.9921M | 53.2326M |
| llvm::DenseMap      | 0.262629 | 23.4997M | 25.4499M | 22.1918M |
| std::unordered_map  | 0.092887 | 8.3114M  | 9.03179M | 7.59683M |

---

## Erase Half — Vec3f (1000)

| Hashmap             | bw_gib_s | cps_avg  | cps_max  | cps_min  |
|---------------------|----------|----------|----------|----------|
| acul::hashmap       | 0.296671 | 26.5457M | 27.1631M | 25.8585M |
| acul::hl_hashmap    | 0.603326 | 53.9847M | 55.6416M | 51.2297M |
| emhash5             | 0.159226 | 14.2473M | 14.7817M | 13.6614M |
| emhash7             | 0.184020 | 16.4658M | 16.9996M | 15.7711M |
| absl::flat_hash_map | 1.39514  | 124.835M | 127.734M | 120.395M |
| llvm::DenseMap      | 0.209791 | 18.7718M | 18.9319M | 18.4311M |
| std::unordered_map  | 0.462846 | 41.4148M | 43.4740M | 37.7660M |

## Erase Half — Vec3f (1,000,000)

| Hashmap             | bw_gib_s | cps_avg  | cps_max  | cps_min  |
|---------------------|----------|----------|----------|----------|
| acul::hashmap       | 0.402966 | 36.0568M | 38.0560M | 33.6532M |
| acul::hl_hashmap    | 0.181766 | 16.2642M | 17.5323M | 15.5002M |
| emhash5             | 0.390382 | 34.9308M | 35.6994M | 33.5959M |
| emhash7             | 0.344669 | 30.8405M | 31.9539M | 29.0560M |
| absl::flat_hash_map | 0.271784 | 24.3188M | 25.8303M | 23.0285M |
| llvm::DenseMap      | 0.374989 | 33.5534M | 36.0077M | 30.5208M |
| std::unordered_map  | 0.045116 | 4.03694M | 4.50932M | 3.61971M |

## Insert After Erase — Vec3f (1000)

| Hashmap             | bw_gib_s | cps_avg  | cps_max  | cps_min  |
|---------------------|----------|----------|----------|----------|
| acul::hashmap       | 0.156603 | 10.5094M | 11.0553M | 10.0246M |
| acul::hl_hashmap    | 2.16637  | 145.383M | 148.389M | 140.897M |
| emhash5             | 0.109599 | 7.35509M | 7.66216M | 6.93951M |
| emhash7             | 0.121816 | 8.17494M | 8.47309M | 7.67952M |
| absl::flat_hash_map | 1.35917  | 91.2122M | 91.9156M | 89.5397M |
| llvm::DenseMap      | 0.018658 | 1.25215M | 1.29136M | 1.22089M |
| std::unordered_map  | 0.502502 | 33.7224M | 36.3221M | 28.859M  |

## Insert After Erase — Vec3f (1,000,000)

| Hashmap             | bw_gib_s | cps_avg  | cps_max  | cps_min  |
|---------------------|----------|----------|----------|----------|
| acul::hashmap       | 0.410029 | 27.5166M | 28.9565M | 26.4580M |
| acul::hl_hashmap    | 0.875471 | 58.7519M | 62.2936M | 54.8792M |
| emhash5             | 0.344639 | 23.1283M | 24.4520M | 21.9330M |
| emhash7             | 0.445583 | 29.9026M | 31.8279M | 27.3456M |
| absl::flat_hash_map | 0.559015 | 37.5149M | 39.7102M | 35.3726M |
| llvm::DenseMap      | 0.144491 | 9.69660M | 10.6944M | 8.98414M |
| std::unordered_map  | 0.068740 | 4.61308M | 4.97939M | 3.97595M |

---

## Erase Half — string (1000)

| Hashmap             | bw_gib_s | cps_avg  | cps_max  | cps_min  |
|---------------------|----------|----------|----------|----------|
| acul::hashmap       | 1.01808  | 68.3221M | 69.7398M | 65.8549M |
| acul::hl_hashmap    | 0.820832 | 55.0851M | 56.1210M | 52.6491M |
| emhash5             | 1.04895  | 70.3941M | 71.4536M | 69.2776M |
| emhash7             | 0.976294 | 65.5180M | 66.6616M | 64.4395M |
| absl::flat_hash_map | 0.939424 | 63.0437M | 64.1031M | 61.8980M |
| llvm::DenseMap      | 0.910164 | 61.0800M | 61.7233M | 60.0393M |
| std::unordered_map  | 0.413292 | 27.7356M | 28.1040M | 27.3404M |

## Erase Half — string (1,000,000)

| Hashmap             | bw_gib_s | cps_avg  | cps_max  | cps_min  |
|---------------------|----------|----------|----------|----------|
| acul::hashmap       | 0.176480 | 11.8433M | 12.0920M | 11.2501M |
| acul::hl_hashmap    | 0.141808 | 9.51659M | 9.83885M | 9.31595M |
| emhash5             | 0.216573 | 14.5340M | 15.0134M | 13.9885M |
| emhash7             | 0.211181 | 14.1721M | 14.9002M | 13.5825M |
| absl::flat_hash_map | 0.176986 | 11.8774M | 12.4839M | 11.5664M |
| llvm::DenseMap      | 0.193595 | 12.9919M | 13.9689M | 12.4741M |
| std::unordered_map  | 0.043874 | 2.94430M | 3.18115M | 2.74716M |

## Insert After Erase — string (1000)

| Hashmap             | bw_gib_s | cps_avg  | cps_max  | cps_min  |
|---------------------|----------|----------|----------|----------|
| acul::hashmap       | 1.70394  | 91.4797M | 93.2526M | 88.8589M |
| acul::hl_hashmap    | 1.92981  | 103.606M | 105.618M | 100.614M |
| emhash5             | 1.66536  | 89.4085M | 91.4017M | 87.9188M |
| emhash7             | 1.79464  | 96.3489M | 98.6043M | 92.5469M |
| absl::flat_hash_map | 1.52263  | 81.7458M | 82.6812M | 80.3752M |
| llvm::DenseMap      | 0.598877 | 32.1520M | 33.0258M | 31.3292M |
| std::unordered_map  | 0.598000 | 32.1049M | 32.6441M | 31.7609M |

## Insert After Erase — string (1,000,000)

| Hashmap             | bw_gib_s | cps_avg  | cps_max  | cps_min  |
|---------------------|----------|----------|----------|----------|
| acul::hashmap       | 0.352805 | 18.9411M | 19.5668M | 17.1480M |
| acul::hl_hashmap    | 0.649654 | 34.8780M | 36.0803M | 33.4353M |
| emhash5             | 0.307416 | 16.5043M | 16.9736M | 16.1583M |
| emhash7             | 0.405493 | 21.7697M | 22.5120M | 20.8813M |
| absl::flat_hash_map | 0.340495 | 18.2802M | 18.7296M | 17.1687M |
| llvm::DenseMap      | 0.149857 | 8.04539M | 8.28528M | 7.76880M |
| std::unordered_map  | 0.056299 | 3.02255M | 3.09709M | 2.94148M |


</details>

<details>
<summary>Iterate</summary>

# Benchmarks — Iterate

## Iterate — uint64_t (1000)

| Hashmap             | bw_gib_s | cps_avg  | cps_max  | cps_min  |
|---------------------|----------|----------|----------|----------|
| acul::hashmap       | 9.29801  | 831.972M | 858.478M | 763.137M |
| acul::hl_hashmap    | 9.63520  | 862.143M | 993.769M | 701.796M |
| emhash5             | 13.7355  | 1.22904G | 1.24136G | 1.21586G |
| emhash7             | 11.3727  | 1.01761G | 1.04300G | 1.00443G |
| absl::flat_hash_map | 2.45518  | 219.685M | 223.843M | 210.435M |
| llvm::DenseMap      | 9.12567  | 816.551M | 830.813M | 802.694M |
| std::unordered_map  | 4.47959  | 400.827M | 406.919M | 396.066M |

## Iterate — uint64_t (1,000,000)

| Hashmap             | bw_gib_s | cps_avg  | cps_max  | cps_min  |
|---------------------|----------|----------|----------|----------|
| acul::hashmap       | 5.52692  | 494.540M | 587.134M | 409.694M |
| acul::hl_hashmap    | 4.92272  | 440.478M | 447.209M | 428.075M |
| emhash5             | 6.63045  | 593.283M | 606.548M | 574.770M |
| emhash7             | 8.02296  | 717.883M | 763.676M | 676.548M |
| absl::flat_hash_map | 2.30410  | 206.167M | 217.874M | 195.470M |
| llvm::DenseMap      | 1.51069  | 135.174M | 138.537M | 128.523M |
| std::unordered_map  | 0.218948 | 19.5911M | 21.1293M | 17.5043M |

---

## Iterate — Vec3f (1000)

| Hashmap             | bw_gib_s | cps_avg  | cps_max  | cps_min  |
|---------------------|----------|----------|----------|----------|
| acul::hashmap       | 5.08306  | 341.119M | 347.341M | 336.287M |
| acul::hl_hashmap    | 4.92952  | 330.814M | 342.945M | 320.376M |
| emhash5             | 5.93988  | 398.618M | 407.899M | 382.972M |
| emhash7             | 6.24444  | 419.057M | 423.401M | 414.613M |
| absl::flat_hash_map | 2.89211  | 194.086M | 200.675M | 190.145M |
| llvm::DenseMap      | 3.33345  | 223.704M | 235.496M | 195.153M |
| std::unordered_map  | 5.53936  | 371.740M | 376.559M | 367.322M |

## Iterate — Vec3f (1,000,000)

| Hashmap             | bw_gib_s | cps_avg  | cps_max  | cps_min  |
|---------------------|----------|----------|----------|----------|
| acul::hashmap       | 1.62626  | 109.136M | 112.054M | 105.846M |
| acul::hl_hashmap    | 4.10561  | 275.523M | 283.030M | 270.601M |
| emhash5             | 1.73220  | 116.246M | 119.692M | 111.276M |
| emhash7             | 3.51388  | 235.812M | 244.501M | 227.532M |
| absl::flat_hash_map | 2.58834  | 173.701M | 177.582M | 169.091M |
| llvm::DenseMap      | 1.41618  | 95.0385M | 96.9701M | 92.3917M |
| std::unordered_map  | 0.174838 | 11.7332M | 12.3543M | 11.2887M |

---

## Iterate — string (1000)

| Hashmap             | bw_gib_s | cps_avg  | cps_max  | cps_min  |
|---------------------|----------|----------|----------|----------|
| acul::hashmap       | 5.06590  | 271.973M | 278.129M | 265.313M |
| acul::hl_hashmap    | 5.39955  | 289.886M | 292.947M | 288.033M |
| emhash5             | 5.51887  | 296.292M | 297.937M | 292.965M |
| emhash7             | 6.07441  | 326.117M | 329.708M | 323.980M |
| absl::flat_hash_map | 3.04588  | 163.524M | 164.580M | 161.505M |
| llvm::DenseMap      | 1.90632  | 102.345M | 107.320M | 97.9285M |
| std::unordered_map  | 6.08351  | 326.606M | 329.296M | 323.970M |

## Iterate — string (1,000,000)

| Hashmap             | bw_gib_s | cps_avg  | cps_max  | cps_min  |
|---------------------|----------|----------|----------|----------|
| acul::hashmap       | 1.76515  | 94.7655M | 97.1510M | 92.5181M |
| acul::hl_hashmap    | 2.94966  | 158.358M | 164.427M | 150.003M |
| emhash5             | 1.68022  | 90.2061M | 92.3322M | 88.5058M |
| emhash7             | 2.95758  | 158.784M | 162.631M | 155.076M |
| absl::flat_hash_map | 2.35850  | 126.621M | 131.625M | 121.184M |
| llvm::DenseMap      | 1.34914  | 72.4313M | 75.4745M | 67.8179M |
| std::unordered_map  | 0.195123 | 10.4756M | 11.1243M | 9.34530M |

</details>
</details>

<details>
<summary>Strings</summary>

| Benchmark             | bw_mib_s / GiB | cps_avg  | cps_max  | cps_min  |
|-----------------------|----------------|----------|----------|----------|
| string_construct_acul | 30.546 Gi      | 2.98167G | 3.0625G  | 2.93281G |
| string_construct_std  | 23.513 Gi      | 2.29516G | 2.34601G | 2.26061G |
| string_append_acul    | 88.677 Mi      | 5.81156M | 6.10836M | 5.43557M |
| string_append_std     | 63.459 Mi      | 4.15883M | 4.32999M | 3.81641M |
| string_find_acul      | 20.163 Gi      | 21.1419M | 21.8026M | 19.7316M |
| string_find_std       | 6.268 Gi       | 6.57268M | 6.8921M  | 6.24759M |
| to_string_u64_acul    | 524.36 Mi      | 68.7289M | 70.1918M | 67.1403M |
| to_string_u64_std     | 778.508 Mi     | 102.041M | 103.336M | 99.2533M |
| stoull_acul           | 2.900 Gi       | 259.486M | 264.768M | 246.967M |
| stoull_std            | 338.466 Mi     | 29.5756M | 30.4536M | 28.6436M |
| sstream_write_acul    | 1.369 Gi       | 22.9704M | 23.6288M | 22.4124M |
| sstream_write_std     | 30.041 Mi      | 492.188k | 492.188k | 492.188k |
| path_join_acul        | 51.805 Mi      | 848.775k | 851.112k | 846.452k |
| path_join_std         | 38.195 Mi      | 625.779k | 633.032k | 618.689k |
| path_decompose_acul   | 145.611 Mi     | 2.3857M  | 2.48344M | 2.31741M |
| path_decompose_std    | 139.709 Mi     | 2.28899M | 2.37451M | 2.19327M |


</details>

<details>

<summary>Smart pointers</summary>

## uint64_t (trivial)

| Benchmark      | Impl             | bw        | cps_avg  | cps_max  | cps_min  |
|----------------|------------------|-----------|----------|----------|----------|
| make_reset     | acul::shared_ptr | 817.367Mi | 107.134M | 111.024M | 103.469M |
|                | std::shared_ptr  | 263.834Mi | 34.581M  | 35.833M  | 32.132M  |
| copy           | acul::shared_ptr | 859.701Mi | 112.683M | 115.088M | 110.476M |
|                | std::shared_ptr  | 265.413Mi | 34.788M  | 36.365M  | 33.284M  |
| move           | acul::shared_ptr | 765.242Mi | 100.302M | 103.017M | 96.198M  |
|                | std::shared_ptr  | 222.171Mi | 29.120M  | 33.790M  | 25.279M  |
| weak_lock_hit  | acul::weak_ptr   | 7.41498Gi | 995.222M | 1.02566G | 955.827M |
|                | std::weak_ptr    | 791.145Mi | 103.697M | 105.165M | 102.097M |
| weak_lock_miss | acul::weak_ptr   | 12.4589Gi | 1.6722G  | 1.778G   | 1.393G   |
|                | std::weak_ptr    | 4.72483Gi | 634.156M | 641.088M | 628.221M |
| deref          | acul::shared_ptr | 27.069Gi  | 3.6331G  | 3.7849G  | 3.4266G  |
|                | std::shared_ptr  | 24.9896Gi | 3.3540G  | 3.4795G  | 3.1709G  |

## NonTriv (non-trivial)

| Benchmark      | Impl             | bw        | cps_avg  | cps_max  | cps_min  |
|----------------|------------------|-----------|----------|----------|----------|
| make_reset     | acul::shared_ptr | 3.00959Gi | 100.985M | 103.833M | 90.766M  |
|                | std::shared_ptr  | 1.05017Gi | 35.238M  | 35.882M  | 34.051M  |
| copy           | acul::shared_ptr | 3.10594Gi | 104.218M | 106.753M | 99.687M  |
|                | std::shared_ptr  | 1.06152Gi | 35.619M  | 36.426M  | 33.789M  |
| move           | acul::shared_ptr | 2.67018Gi | 89.596M  | 92.914M  | 86.628M  |
|                | std::shared_ptr  | 994.88Mi  | 32.600M  | 33.387M  | 31.908M  |
| weak_lock_hit  | acul::weak_ptr   | 26.7828Gi | 898.681M | 939.974M | 870.236M |
|                | std::weak_ptr    | 3.21253Gi | 107.795M | 110.760M | 104.762M |
| weak_lock_miss | acul::weak_ptr   | 43.6595Gi | 1.4650G  | 1.472G   | 1.452G   |
|                | std::weak_ptr    | 19.4999Gi | 654.308M | 660.433M | 648.618M |
| deref          | acul::shared_ptr | 53.4278Gi | 1.7927G  | 1.8393G  | 1.727G   |
|                | std::shared_ptr  | 48.0888Gi | 1.6136G  | 1.6527G  | 1.549G   |


</details>

## License
This project is licensed under the [MIT License](LICENSE).

## Contacts
For any questions or feedback, you can reach out via [email](mailto:wusikijeronii@gmail.com) or open a new issue.