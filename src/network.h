#pragma once

#include "Node.h"
#include "Sampler.h"
#include "unordered_map"
#include "vector_helpers.h"
#include <unordered_map>

using Node_UPtr_Vec = std::vector<Node_UPtr>;
using Type_Vec      = std::vector<std::vector<Node_UPtr>>;

template <typename T>
using String_Map = std::unordered_map<string, T>;

struct State_Dump {
  std::vector<string> ids;
  std::vector<string> types;
  std::vector<string> parents;
  std::vector<int> levels;
  State_Dump(const int size)
  {
    ids.reserve(size);
    types.reserve(size);
    parents.reserve(size);
    levels.reserve(size);
  }
  int size() const { return ids.size(); }
};

class SBM_Network {

  private:
  // Data
  std::vector<Type_Vec> nodes;
  std::vector<string> types;
  Int_Map<string> type_name_to_int;
  Sampler random_sampler;
  int block_counter = 0; // Keeps track of how many block we've had

  // =========================================================================
  // Private helper methods
  // =========================================================================
  int get_type_index(const string name) const
  {
    const auto name_it = type_name_to_int.find(name);

    // If this is a previously unseen type, add new entry
    if (name_it == type_name_to_int.end())
      LOGIC_ERROR("Type " + name + " doesn't exist in network");

    return name_it->second;
  }

  void check_for_level(const int level) const
  {
    // Make sure we have the requested level
    if (level >= nodes.size())
      RANGE_ERROR("Node requested in level that does not exist");
  }

  void check_for_type(const int type_index) const
  {
    if (type_index >= num_types())
      RANGE_ERROR("Type " + as_str(type_index) + " does not exist in network.");
  }

  public:
  // =========================================================================
  // Constructor
  // =========================================================================
  SBM_Network(const std::vector<std::string>& node_types = { "node" },
              const int random_seed                      = 42)
      : random_sampler(random_seed)
      , types(node_types)
  {
    int c_index = 0;
    for (const auto& type_name : node_types) {
      type_name_to_int[type_name] = c_index++;
    }

    build_level();
  }

  // =========================================================================
  // Information
  // =========================================================================
  int num_nodes() const
  {
    return total_num_elements(nodes);
  }

  int num_nodes_at_level(const int level) const
  {
    check_for_level(level);
    return total_num_elements(nodes.at(level));
  }

  int num_levels() const
  {
    return nodes.size();
  }

  int num_nodes_of_type(const int type_i, const int level = 0) const
  {
    check_for_level(level);
    check_for_type(type_i);
    return nodes.at(level).at(type_i).size();
  }

  int num_nodes_of_type(const string& type, const int level = 0) const
  {
    check_for_level(level);
    return nodes.at(level).at(get_type_index(type)).size();
  }

  int num_types() const
  {
    return types.size();
  }

  // Export current state of nodes in model
  State_Dump get_state()
  {
    if (num_levels() == 1)
      LOGIC_ERROR("No state to export - Try adding blocks");

    // Initialize the return struct
    State_Dump state(num_nodes());

    // No need to record the last level's nodes as they are already included
    // in the previous node's parent slot
    for (int level = 0; level < num_levels() - 1; level++) {
      for_all_nodes_at_level(level, [&](const Node_UPtr& node) {
        state.ids.push_back(node->get_id());
        state.types.push_back(types[node->get_type()]);
        state.parents.push_back(node->get_parent_id());
        state.levels.push_back(level);
      });
    }

    return state;
  }

  // Apply a lambda function over all nodes in network
  void for_all_nodes_at_level(const int level,
                              std::function<void(const Node_UPtr& node)> fn) const
  {
    check_for_level(level);
    for (const auto& nodes_vec : nodes.at(level)) {
      std::for_each(nodes_vec.begin(), nodes_vec.end(), fn);
    }
  }

  // =========================================================================
  // Modification
  // =========================================================================
  Node* add_node(const std::string& id,
                 const std::string& type = "a",
                 const int level         = 0)
  {
    const int type_index = get_type_index(type);

    // Build new node pointer outside of vector first for ease of pointer retrieval
    auto new_node = Node_UPtr(new Node(id, level, type_index, num_types()));

    // Get raw pointer to node to return
    Node* node_ptr = new_node.get();

    // Move node unique pointer into its type in map
    get_nodes_of_type(type_index, level).push_back(std::move(new_node));

    return node_ptr;
  }

  void initialize_blocks(int num_blocks = -1)
  {
    const bool one_block_per_node = num_blocks == -1;
    const int block_level         = num_levels();
    const int child_level         = block_level - 1;

    // Build empty level
    build_level();

    // Loop over all node types
    for (int type_i = 0; type_i < num_types(); type_i++) {

      Node_UPtr_Vec& nodes_of_type  = get_nodes_of_type(type_i, child_level);
      Node_UPtr_Vec& blocks_of_type = get_nodes_of_type(type_i, block_level);

      // If we're in the 1-block-per-node mode make sure we reflect that in reserved size
      if (one_block_per_node)
        num_blocks = nodes_of_type.size();

      if (num_blocks > nodes_of_type.size())
        LOGIC_ERROR("Can't initialize more blocks than there are nodes of a given type");

      // Reserve enough spaces for the blocks to be inserted
      blocks_of_type.reserve(num_blocks);

      for (int i = 0; i < num_blocks; i++) {
        // Build a new block node wrapped in smart pointer in it's type vector
        blocks_of_type.emplace_back(new Node(block_counter++, type_i, block_level, num_types()));
      }

      // Shuffle child nodes if we're randomly assigning blocks
      if (!one_block_per_node)
        random_sampler.shuffle(nodes_of_type);

      // Loop through now shuffled children nodes
      for (int i = 0; i < nodes_of_type.size(); i++) {
        // Add blocks one at a time, looping back after end to each node
        nodes_of_type[i]->set_parent(blocks_of_type[i % num_blocks].get());
      }
    }
  }

  void build_level()
  {
    nodes.emplace_back(num_types());
  }

  void delete_block_level()
  {
    if (has_blocks()) {
      // Remove the last layer of nodes.
      nodes.pop_back();
    } else {
      LOGIC_ERROR("No block level to delete.");
    }
  }

  void delete_all_blocks()
  {
    while (has_blocks()) { delete_block_level();}
  }

  bool has_blocks() const {
    return num_levels() > 1;
  }

  // =============================================================================
  // Load current state of nodes in model from state dump given SBM::get_state()
  // =============================================================================
  void update_state(const std::vector<string>& ids,
                    const std::vector<string>& parents,
                    const std::vector<int>& levels,
                    const std::vector<string>& types)
  {
    delete_all_blocks(); // Remove all block levels
    build_level();       // Add an empty block level to fill in

    // Build a map to get nodes by id
    String_Map<Node*> node_by_id;
    auto add_node_to_map = [&node_by_id](const Node_UPtr& node) {
      node_by_id[node->get_id()] = node.get();
    };
    for_all_nodes_at_level(0, add_node_to_map);

    // Setup map to get blocks/parents by id
    String_Map<Node*> block_by_id;

    // Loop through entries of the state dump
    int last_level = 0;
    for (int i = 0; i < ids.size(); i++) {
      const string& id     = ids[i];
      const string& parent = parents[i];
      const string& type   = types[i];
      const int level      = levels[i];

      // If the level of the current entry has gone up
      // Swap the maps as the blocks are now the child nodes
      if (last_level != level) {
        node_by_id = std::move(block_by_id); // block_by_id will be empty now
        build_level();                       // Setup new level for blocks
        last_level = level;                  // Update the current level
      }

      // Find current entry's node
      Node* current_node = [&]() {
        const auto node_it = node_by_id.find(id);
        if (node_it == node_by_id.end())
          LOGIC_ERROR("Node in state (" + id + ") is not present in network");
        return node_it->second;
      }();

      // Grab parent block pointer
      Node* parent_node = [&]() {
        const auto parent_it = block_by_id.find(parent);
        // If this block is newly seen, create it
        if (parent_it == block_by_id.end())
          return block_by_id.emplace(parent, add_node(parent, type, level + 1)).first->second;
        return parent_it->second;
      }();

      // Connect node and parent to eachother
      current_node->set_parent(parent_node);
    }
  }

  void update_state(const State_Dump& state)
  {
    update_state(state.ids, state.parents, state.levels, state.types);
  }

  // =========================================================================
  // Node Grabbers
  // =========================================================================
  Type_Vec& get_nodes_at_level(const int level = 0)
  {
    check_for_level(level);
    return nodes.at(level);
  }

  Node_UPtr_Vec& get_nodes_of_type(const int type_index, const int level = 0)
  {
    check_for_type(type_index);
    return get_nodes_at_level(level)[type_index];
  }

  Node_UPtr_Vec& get_nodes_of_type(const std::string& type, const int level = 0)
  {
    return get_nodes_of_type(get_type_index(type), level);
  }

  Node* get_node_by_id(const string& id, const string& type)
  {
    // Slow. To be only used in testing/debugging
    auto& nodes_of_type = get_nodes_of_type(type);

    return std::find_if(
               nodes_of_type.begin(),
               nodes_of_type.end(),
               [&id](Node_UPtr& node) { return node->get_id() == id; })
        ->get();
  }
};