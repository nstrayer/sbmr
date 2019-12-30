#include "Network.h" 

// =============================================================================
// Setup a new Node level
// =============================================================================
void Network::add_level(const int level) 
{
  PROFILE_FUNCTION();
  // First, make sure level doesn't already exist
  if (nodes.find(level) == nodes.end()) {
    // Setup first level of the node map
    nodes.emplace(level, std::make_shared<NodeLevel>());
  }
  
}


// =============================================================================
// Grab reference to a desired level map. If level doesn't exist yet, it will be
// created
// =============================================================================
LevelPtr Network::get_level(const int level) 
{
  PROFILE_FUNCTION();
  // Grab level for block node
  LevelMap::iterator block_level = nodes.find(level);

  // Is this a new level?
  bool level_doesnt_exist = block_level == nodes.end();

  if (level_doesnt_exist) {
    // Add a new node level
    add_level(level);

    // 'find' that new level
    block_level = nodes.find(level);
  }
  
  return block_level->second;
}


// =============================================================================
// Find and return a node by its id
// =============================================================================
NodePtr Network::get_node_by_id(const string desired_id, const int level) 
{
  PROFILE_FUNCTION();
  try {
    // Attempt to find node on the 'node level' of the Network
    return nodes.at(level)->at(desired_id);
  } catch (...) {
    // Throw informative error if it fails
    std::cerr << "Could not find requested node" << std::endl;
    throw "Could not find requested node";
  }
  
}


// =============================================================================
// Builds a block id from a scaffold for generated new blocks
// =============================================================================
string Network::build_block_id(const int type,
                               const int level,
                               const int index)
{
  PROFILE_FUNCTION();
  return std::to_string(type)  + "-" +
    std::to_string(level) + "_" +
    std::to_string(index);
}


// =============================================================================
// Adds a node with an id and type to network
// =============================================================================
NodePtr Network::add_node(const string id,
                          const int type,
                          const int level)
{
  PROFILE_FUNCTION();
  // Grab level
  LevelPtr node_level = get_level(level);

  // Check if we need to make the id or not
  string node_id = id == "new block" ?
    build_block_id(type, level, node_level->size()):
    id;
  
  // Create node
  NodePtr new_node = std::make_shared<Node>(node_id, level, type);
  
  (*node_level)[node_id] = new_node;
 
  // Add this node to node counting map
  node_type_counts[type][level]++;

  
  return new_node;
}; 


// =============================================================================
// Creates a new block node and add it to its neccesary level
// =============================================================================
NodePtr Network::create_block_node(const int type, const int level) 
{
  PROFILE_FUNCTION();

  // Make sure requested level is not 0
  if(level == 0) {
    std::cerr << "Can't create block node at first level" << std::endl;
    throw "Can't create block node at first level";
  }
  
  // Initialize new node
  return add_node("new block", type, level);
};


// =============================================================================
// Return nodes of a desired type from level. If match_type = true then the
// nodes returned are of the same type as specified, otherwise the nodes
// returned are _not_ of the same type.
// =============================================================================
std::vector<NodePtr> Network::get_nodes_from_level(const int type,
                                                   const int level,
                                                   const bool match_type)
{
  PROFILE_FUNCTION();
  // Grab desired level reference
  LevelPtr node_level = nodes.at(level);
  
  // Make sure level has nodes before looping through it
  if (node_level->size() == 0) 
  {
    std::cerr << "Requested level " << level << " is empty of nodes of type " 
              << type << " when " << (match_type ? "": "not ") << "matching type" 
              << std::endl;
    throw "Requested level is empty.";
  }
  
  // Where we will store all the nodes found from level
  std::vector<NodePtr> nodes_to_return;
  nodes_to_return.reserve(node_level->size());

  // Loop through every node belonging to the desired level
  for (auto node_it  = node_level->begin(); 
            node_it != node_level->end(); 
            ++node_it) 
  {
    // Decide to keep the node or not based on if it matches or doesn't and our
    // keeping preferance
    bool keep_node = match_type ? 
    (node_it->second->type == type) : 
    (node_it->second->type != type);
    
    if(keep_node) {
      // ...Place it in returning list
      nodes_to_return.push_back(node_it->second);
    }
  }
  
  return nodes_to_return;
} 


// =============================================================================
// Return nodes of a desired type from level. 
// =============================================================================
std::vector<NodePtr> Network::get_nodes_of_type_at_level(const int type, const int level) 
{
  PROFILE_FUNCTION();
  return get_nodes_from_level(type, level, true);
}   


// =============================================================================
// Return nodes _not_ of a specified type from level
// =============================================================================
std::vector<NodePtr> Network::get_nodes_not_of_type_at_level(const int type, const int level) 
{
  PROFILE_FUNCTION();
  return get_nodes_from_level(type, level, false);
}   


// =============================================================================
// Adds a edge between two nodes based on their ids
// =============================================================================
void Network::add_edge(const string node1_id, const string node2_id) 
{
  PROFILE_FUNCTION();
  
  Node::connect_nodes(
    get_node_by_id(node1_id), 
    get_node_by_id(node2_id)
  );
  
};    


// =============================================================================
// Adds a edge between two nodes based on their references
// =============================================================================
void Network::add_edge(const NodePtr node1, const NodePtr node2) 
{
  PROFILE_FUNCTION();
  Node::connect_nodes(
    node1, 
    node2
  );
  
};  


// =============================================================================
// Builds and assigns a block node for every node in a given level
// =============================================================================
void Network::give_every_node_at_level_own_block(const int level) 
{ 
  initialize_blocks(-1, level);
}    

// =============================================================================
// Adds a desired number of blocks and randomly assigns them for a given level
// num_blocks = -1 means every node gets their own block
// =============================================================================
void Network::initialize_blocks(const int num_blocks, const int level)
{
  PROFILE_FUNCTION();

  const int block_level = level + 1;

  // Clear all previous nodes in block level out
  get_level(block_level)->clear();

  // Grab all the nodes for the desired level
  LevelPtr node_level = nodes.at(level);

  const int num_nodes_in_level = node_level->size();

  // Setup a sampler
  Sampler block_sampler;

  // Make sure level has nodes before looping through it
  if (num_nodes_in_level == 0) 
  {
    std::cerr << "Requested level is empty. (initialize_blocks())" << std::endl;
    throw "Requested level is empty.";
  }

  // Figure out how we're making blocks, is it one block per node or a set number
  // of blocks total?
  bool one_block_per_node = num_blocks == -1;

  // Make a map that gives us type -> array of new blocks
  std::map<int, std::vector<NodePtr>> type_to_blocks;

  // If we're randomly distributing nodes, we'll use this map to sample a random
  // block for a given node by its type
  if (!one_block_per_node) 
  {
    for (auto type_it = node_type_counts.begin();
              type_it != node_type_counts.end();
              type_it++)
    {
      // Reserve proper number of slots for new blocks
      type_to_blocks[type_it->first].reserve(num_blocks);

      // Buid new blocks to fill those slots
      for (int i = 0; i < num_blocks; i++)
      {
        // build a block node at the next level
        type_to_blocks[type_it->first].push_back(create_block_node(type_it->first, level + 1));
      }
    }
  }

  // Loop through every node in the level and assign it its new parent
   // Loop through each of the nodes,
  for (auto node_it  = node_level->begin(); 
            node_it != node_level->end(); 
            ++node_it) 
  {
    const int node_type = node_it->second->type;

    // build a block node at the next level
    // We either build a new block for node if we're giving each node a block
    // or sample new block from available list of blocks for this type
    NodePtr new_block = one_block_per_node ?
      create_block_node(node_type, level + 1):
      block_sampler.sample(type_to_blocks[node_type]);
      
    // assign that block node to the node
    node_it->second->set_parent(new_block);
  }
}



// =============================================================================
// Grabs the first node found at a given level, used in testing.
// =============================================================================
NodePtr Network::get_node_from_level(const int level) 
{
  PROFILE_FUNCTION();
  return nodes.at(level)->begin()->second;
}



// =============================================================================
// Scan through entire Network and remove all block nodes that have no children. 
// Returns the number removed
// =============================================================================
std::vector<NodePtr> Network::clean_empty_blocks()
{
  PROFILE_FUNCTION();
  int num_levels = nodes.size();
  int total_deleted = 0;
  
  std::vector<NodePtr> blocks_removed;

  
  // Scan through all levels up to final
  for (int level = 1; level < num_levels; ++level) 
  {
    // Grab desired level
    LevelPtr block_level = nodes.at(level);
    
    // Create a vector to store block ids that we want to delete
    std::queue<string> blocks_to_delete;
    
    // Loop through every node at level
    for (auto block_it = block_level->begin(); 
              block_it != block_level->end(); 
              ++block_it)
    {
      NodePtr current_block = block_it->second;
      
      // If there are no children for the current block
      if (current_block->children.size() == 0) 
      {
        // Remove block from children of its parent (if it has one)
        if (current_block->parent) 
        {
          current_block->parent->remove_child(current_block);
        }

        blocks_removed.push_back(current_block);

        // Add current block to the removal list
        blocks_to_delete.push(current_block->id);

        // Remove nodes contribution to node counts map
        node_type_counts[current_block->type][level]--;
      }
    }

    // Remove all the blocks in the removal list
    while (!blocks_to_delete.empty())
    {
      block_level->erase(blocks_to_delete.front());
      
      // Remove reference from queue
      blocks_to_delete.pop();

      // Increment total blocks deleted counter
      total_deleted++;
    }
  }
  
  return blocks_removed;
}                     




// =============================================================================
// Export current state of nodes in model
// =============================================================================
State_Dump Network::get_state()
{
  PROFILE_FUNCTION();
  // Initialize the return struct
  State_Dump state; 
  
  // Keep track of how many nodes we've seen so we can preallocate vector sizes
  int n_nodes_seen = 0;
  
  // Loop through all the levels present in Network
  for (auto level_it = nodes.begin(); 
            level_it != nodes.end(); 
            ++level_it) 
  {
    int level = level_it->first;
    LevelPtr node_level = level_it->second;
    
    // Add level's nodes to current total
    n_nodes_seen += node_level->size();
    
    // Update sizes of the state vectors
    state.id.reserve(n_nodes_seen);
    state.level.reserve(n_nodes_seen);
    state.parent.reserve(n_nodes_seen);
    state.type.reserve(n_nodes_seen);
    
    // Loop through each node in level
    for (auto node_it = node_level->begin(); 
              node_it != node_level->end(); 
              ++node_it )
    {
      // Get currrent node
      NodePtr current_node = node_it->second;

      // Dump all its desired info into its element in the state vectors
      state.id.push_back(current_node->id);
      state.level.push_back(level);
      state.type.push_back(current_node->type);
      
      // Record parent if node has one
      state.parent.push_back(
        current_node->parent ? current_node->parent->id: "none"
      );
      
    } // End node loop
  } // End level loop
  
  return state;
}


// =============================================================================
// Load current state of nodes in model from state dump given Network::get_state() 
// =============================================================================
void Network::load_from_state(const State_Dump state)
{ 
  PROFILE_FUNCTION();

  int n = state.parent.size();

  for (int i = 0; i < n; i++)
  {
    int node_type = state.type[i];

    string child_id = state.id[i];
    int child_level = state.level[i];
    
    string parent_id = state.parent[i];
    int parent_level = child_level + 1;

    auto aquire_node = [node_type, this](string node_id, int node_level) {
      LevelPtr nodes_at_level = get_level(node_level);

      // Attempt to find the node in the network
      auto node_loc = nodes_at_level->find(node_id);

      if (node_loc == nodes_at_level->end())
      {
        return add_node(node_id, node_type, node_level);
      }
      else
      {
        return node_loc->second;
      }
    };

    // "none" indicates the highest level has been reached
    if (parent_id == "none") continue;

    // Attempt to find the parent node in the network
    NodePtr parent_node = aquire_node(parent_id, parent_level);

    // Next grab the child node (this one should exist...)
    NodePtr child_node = aquire_node(child_id, child_level);

    // Assign the parent node to the child node
    child_node->set_parent(parent_node);
  }

  // Now clean up any potentially childless nodes that got kicked
  // out by this process
  clean_empty_blocks();
}
