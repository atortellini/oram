# What is this?

This repo contains two implementations of [ORAM schemes](#problem-definition) with an experiment harness for both. The experiment harness exercises its respective ORAM implementation through a configurable number of range queries of an equally configurable size. The experiment harness records and writes to stdout the latency observed for each range query performed. Given the range size `r` (which is a non-zero power of two) and the number of accesses `a` (which is also a non-zero power of two), the sequence of accesses performed will be `a` accesses for each of the `lg(r) + 1`, power of two, range sizes.

# Parameters

Configure the parameters of both ORAM implementations and their experiment harnesses through the macros defined in `parameters/parameters.h`.

# Compilation
Use the CMake file to compile both `path_oram_test` and `roram_test` binaries.

```bash
mkdir build
cd build
cmake ..
make
```

# Problem Definition:
ORAM seeks to address the issue of a trusted client wishing to store and retrieve blocks of data on an untrusted server while minimizing the amount of information revealed about said data. Although other schemes exist to attempt at addressing this issue (e.g. Searchable Encryption schemes), many of these schemes reveal access patterns and search patterns upon retrieval/queries, both of which can be exploited to reveal significant information about the data being stored. Specifically, access pattern leakage encompasses the number of blocks being returned for a particular query, the encrypted identifiers/contents of the blocks consistently returned for a query, and the potential overlap of encrypted blocks with other encrypted queries. Search pattern leakage refers to the property of a scheme which allows an adversary to discern whether a keyword has already been requested by the client based on the history of encrypted queries it receives. To this end, ORAM intends to reveal no useful information about the search pattern and access pattern of blocks. That is, the sequence of blocks accessed as a result of any two, equal-length sequences of reads and/or writes are completely indistinguishable from each other. This project relates to implementing Path ORAM and an extension, rORAM. Path ORAM is a practical implementation of an ORAM algorithm with relatively small, client-side storage requirements. Although this scheme is effective at addressing the aforementioned issue, it suffers greatly in performance for not being locality-aware when accessing logically sequential blocks (e.g. like when performing range queries). rORAM is an extension to the Path ORAM implementation that looks to address this exact issue through imposing a locality-aware bucket layout on disk, and maintaining a collection of subORAMs which support distinct range queries. In doing so, rORAM relaxes the ORAM security definition, allowing the approximate size of the range to be leaked as a tradeoff to minimize the locality when performing range queries for logically sequential blocks (which they argue is already leaked regardless in most applications).
 # Threat Model:
ORAM works in the Client-Server model, where a trusted client delegates the storage of their data to an untrusted server, requiring some form of communication for the client to retrieve their data along with cooperation from the server. In this case, the server is assumed to be honest-but-curious. This means the server is expected to strictly adhere to the specification of the scheme, but will observe any and all information that is leaked when the client interacts with it. With respect to the Path ORAM and rORAM implementation, the server is able to observe which paths are being touched. That is, the server can see the encrypted collection of blocks (buckets) that are requested. The server is also aware of the total number of real blocks that are stored. Specific to rORAM, the server can also observe the approximate range size that is being requested.
