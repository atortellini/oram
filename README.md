For now my idea is to make a target in a Makefile that will compile an experiment harness which will compile both the Path ORAM implementation, and the rORAM implementation, allowing for a program which can provide an easy interface for adjusting the parameters of either implementation, and returning timing information for the sake of experimentation
Already thinking about the structure of the program for experimentation probably have a CLI with flags:
  -n [0-9]+:      specifies 2^n as the number of real data blocks to be stored in the ORAM (e.g. -n 20 will be 2^20 real blocks)
  -r [1-9]+:      specifies the range of [1, num] that will be used to query the ORAM implementation (defaults to 1 for point queries)
  -i path|roram:  specifies which ORAM implementation should be used for the run; either the base Path ORAM 'path' or the rORAM extension 'roram'
                  (defaults to path)
  -z [1-9]+:      specifies the number of blocks that should be contained wihtin each bucket (defaults to 4 blocks per bucket)
  -b [1-9]+:      specifies the block size in bytes (defaults to 8 bytes per block)


Path ORAM:
Definitions:
Perfect Binary Tree (PBT) - A binary tree such that every non-leaf node has two children and every leaf node is found at the same depth/level.
Path ID - Since each leaf node in a binary tree designates a unique path, this can be thought of as the unique number identifying a leaf node where 0 denotes the first leaf node, 1 denotes the second leaf node, etc.
Node ID - The unique number identifying a node within a binary tree (same as the number identifying a node in a heap/flat binary tree construction) beginning from 0 for the root, 1 for its left child, 2 for its right child, etc.

Block:
  Represented as a 3-tuple (a, x, data), where a is the block ID, x is the path mapped to, data is the client data

Experimentation Harness:
  

Client:

  - Stash:
  Set it up as a vector/dynamic array. Can't be a static array as that does not easily support the removal of elements (blocks in this instance) that occurs for those that are evicted back into the server's PBT.
  Instead as a simple vector/dynamic array can have the type be of a block and easily iterate through the stash during an Access to find the target block. This also makes it incredibly easy when going to evict a block as you would simply iterate through every level of the PBT for every block in the stash and create a new vector for all of the potential blocks to be incorporated in the bucket at the current level, removing their reference from the stash. Then, simply iterate through all blocks in the potential_blocks and when/if Z blocks are reached, add any of the remaining potential blocks back into the stash.
  This is the simplest implementation that I've thought of where you don't have to bother with using sets or having to devise an algorithm to update the stash to only contain blocks which have not already been written into the bucket (like performing a set difference between the stash and the potential buckets).

  - K: Some type of secret key for the randomized encryption scheme
  - L: number of levels in PBT derived from CLI -n argument
  - N: number of real blocks/leaf nodes = 2 << L
  - Z: number of blocks per bucket derived from CLI -z argument, defaults to 4.
  - B: number of bytes per block derived from CLI -b argument, defaults to 8 bytes.
  - Stash: vector/dynamic array of blocks; would actually probably be better as a set that supports mathematical set operations/methods
  - Position Map: Could be hash map but for sake of this implementation can just be a normal array of int[N] withreal block IDs are just mapped from [0, N-1].
  - Singleton dummy block (a, x, data):
    - a: N+1 or some other value that does not collide with the real block IDs
    - x: Does't really matter tbh
    - data: garbage data

  - Setup algorithm:
    ````C
    for (int block_id=0; block_id < N; block_id++) {
      position_map[block_id] = uniform_random(0, N-1); // Map block id to a random path id
    }
    ````
    Encrypt/Decrypt Buckets:
      Two choices for these implementations that depend on the server implementation:
        - Can choose to implement encrypting/decrypting each block in a bucket individually and server maintains array of encrypted blocks per bucket
          - Believe this implementation is most similar to what was specified in the paper
        - Can treat a bucket as an atomic unit and encrypt/decrypt it atomically, server only maintains array of encrypted buckets and notion of buckets only exists for the client
  - encryptBucket(blocks):
      bucket = rnd_encrypt(K, blocks)
      return bucket
  - decryptBucket(bucket):
      blocks = rnd_decrypt(K, bucket)
      return blocks

  - ReadBucket(bucket_ID):
      bucket = server_read(bucket_ID)
      decrypted_blocks[] = decryptBucket(bucket)
      blockIsntDummy = (block) => block.block_id !== dummy_block.block_id
      real_blocks = decrypted_blocks.filter(blockIsntDummy)
      return real_blocks

  - WriteBucket(bucket_ID, blocks[]):
      bucketIsntFull = blocks.length < Z
      if (bucketIsntFull):
        dummies_to_make = Z - blocks.length
        while (dummies_to_make) {
          blocks.append(dummy_block);
          dummies_to_make--;
        }
      encrypted_bucket = encryptBucket(blocks)
      server_write(bucket_ID, encrypted_bucket)


After having just ran Setup, initializing the position map populating the server with 2N - 1 buckets full of Z dummy blocks, it would be the case that on first access, no block with block id a will be found depsite having a path mapping.
  - Access(a, op, data) func:
      target_block_id = a
      path_id = position_map[block_id]

      for (uint level = 0; level <= L; level++) {
        stash.append(ReadBucket(P(path_id, level)))
      }

      isTargetBlock = (block) => target_block_id == block.block_id
      target_block = stash.find(isTargetBlock)
      return_data = target_block?.data ?? 0
      
      new_path_id = uniform_random(0, N-1)
      firstTimeAccess = target_block == NULL
      if (firstTimeAccess) {
        target_block = new block(target_block_id, new_path_id, 0);
      } else {
        target_block.path_id = new_path_id;
      }

      if (op == WRITE) {
        target_block.data = data;
      }

      for (uint level = L; level >= 0; level--) {
        potential_blocks = new std::vector<blocks>()
        evicted_bucket_id = P(path_id, level)

        for (block& : stash) {
          blockPathIntersectsEvictedPath = evicted_bucket_id == P(block.path_id, level)
          // just make an array[Z] on the stack and store Z sets that intersect, keep a counter and break when the counter is equal to Z. Then just iterate through the array from 0 to the counter-1, deleting each block from the stash (which can be done by simply swapping the block at the designated index with the stash counter-1 and decrementing the stash counter). This 'deletion' should probably occur after the blocks have been written to the server storage, although it doens't actually matter since no data is actually zero'd.
          if (blockPathIntersectsEvictedPath) {
            potential_blocks.append(block)
            stash.erase(block); // idk something like this where I will iteratively remove every potential block to be added to the current bucket from the stash
          }
        }
        
        new_bucket = potential_blocks

        // If there are greater than Z blocks that can be fit within the bucket, pop the excess blocks from the vector of blocks to be written to the bucket and instead introduce them back into the stash.
        if (potential_blocks.length > Z) {
          for (int excess_block_index = Z; excess_block_index < potential_blocks.length; excess_block_index++) {
            stash.append(new_bucket[excess_block_index]);
            new_bucket.erase(excess_block_index);
        }
        WriteBucket(bucket_id, new_bucket)
        del potential_blocks;
      }

      return return_data;

    
  - P(x,l) algorithm:
    - Parameter x denotes the path id
    - l denotes the level/depth in the PBT
    - The total number of levels, L, in the PBT should either be an additional parameter or stored in some shared state
    Implementation:
    ````C
      const uint total_num_leaves = 2 << L
      const uint nodes_at_level_l = 2 << l
      const uint num_leaves_per_node_at_level_l = total_num_leaves / num_nodes_at_level_l
      const uint node_containing_leaf_x = x / num_leaves_per_node_at_level_l // Make sure its truncating division
      const uint num_nodes_above_level_l = (2 << l) - 1
      const uint final_node_id = node_containing_leaf_x + num_nodes_above_level_l
      return final_node_id
    ````
    
    
Server:

  Setup func or something, paper is a little ambiguous about how the server becomes initialized with 2N-1 buckets full of randomized encryptions of the dummy block. My thoughts is during setup to just repeatedly invoke WriteBucket with empty buckets/sets of blocks as the prcoedure pads with randomized encyrptions of the dummy block anyways. So, would simply invoke WriteBucket for every bucket from 0 to 2N-2 and that will have satisfied the initialization procedure for the server beginning with all buckets full of dummy blocks.

  Setup():
    total_buckets = 2 * N - 1
    for (int bucket_id=0; bucket_id < total_buckets; bucket_id++) {
      Client.WriteBucket(bucket_id, [])
    }
  
  WriteBucket(bucket_id, bucket):
    bucket_arr[bucket_id] = bucket
  ReadBucket(bucket_id):
    return bucket_arr[bucket_id];

  Or it could be that, if the implementation is such that the server is aware of the blocks in the bucket structure and the blocks are encrypted independently, rather than just encrypting the entire bucket you could have:

  WriteBucket(bucket_id, blocks):
    target_bucket_arr = bucket_arr[bucket_id];
    for (int i=0;i < Z;i++) {
      target_bucket[i] = blocks[i]
    }


  ReadBucket(bucket_id):
    return bucket_arr[bucket_id];


  Or some variation of this depending on how I decide to structure the blocks and buckets. 


  struct Bucket {
    std::vector<struct Block> blocks;
  }


  struct Block {
    int block_id;
    int path_id;
    std::vector<uint8_t> data;
  }