#include "mappings.h"
#include "FN_multi_functions.h"

namespace FN {

template<typename T, typename... Args>
T &allocate_resource(const char *name, OwnedResources &resources, Args &&... args)
{
  std::unique_ptr<T> value = BLI::make_unique<T>(std::forward<Args>(args)...);
  T &value_ref = *value;
  resources.add(std::move(value), name);
  return value_ref;
}

static void INSERT_vector_math(VTreeMFNetworkBuilder &builder,
                               OwnedResources &resources,
                               const VTreeMultiFunctionMappings &UNUSED(mappings),
                               const VNode &vnode)
{
  const MultiFunction &fn = allocate_resource<FN::MF_AddFloat3s>("vector math function",
                                                                 resources);
  builder.add_function(fn, {0, 1}, {2}, vnode);
}

static const MultiFunction &get_vectorized_function(
    const MultiFunction &base_function,
    OwnedResources &resources,
    PointerRNA *rna,
    ArrayRef<const char *> is_vectorized_prop_names)
{
  Vector<bool> input_is_vectorized;
  for (const char *prop_name : is_vectorized_prop_names) {
    char state[5];
    RNA_string_get(rna, prop_name, state);
    BLI_assert(STREQ(state, "BASE") || STREQ(state, "LIST"));

    bool is_vectorized = STREQ(state, "LIST");
    input_is_vectorized.append(is_vectorized);
  }

  if (input_is_vectorized.contains(true)) {
    return allocate_resource<FN::MF_SimpleVectorize>(
        "vectorized function", resources, base_function, input_is_vectorized);
  }
  else {
    return base_function;
  }
}

static void INSERT_float_math(VTreeMFNetworkBuilder &builder,
                              OwnedResources &resources,
                              const VTreeMultiFunctionMappings &UNUSED(mappings),
                              const VNode &vnode)
{
  const MultiFunction &base_fn = allocate_resource<FN::MF_AddFloats>("float math function",
                                                                     resources);
  const MultiFunction &fn = get_vectorized_function(
      base_fn, resources, vnode.rna(), {"use_list__a", "use_list__b"});

  builder.add_function(fn, {0, 1}, {2}, vnode);
}

static void INSERT_combine_vector(VTreeMFNetworkBuilder &builder,
                                  OwnedResources &resources,
                                  const VTreeMultiFunctionMappings &UNUSED(mappings),
                                  const VNode &vnode)
{
  const MultiFunction &base_fn = allocate_resource<FN::MF_CombineVector>("combine vector function",
                                                                         resources);
  const MultiFunction &fn = get_vectorized_function(
      base_fn, resources, vnode.rna(), {"use_list__x", "use_list__y", "use_list__z"});
  builder.add_function(fn, {0, 1, 2}, {3}, vnode);
}

static void INSERT_separate_vector(VTreeMFNetworkBuilder &builder,
                                   OwnedResources &resources,
                                   const VTreeMultiFunctionMappings &UNUSED(mappings),
                                   const VNode &vnode)
{
  const MultiFunction &base_fn = allocate_resource<FN::MF_SeparateVector>(
      "separate vector function", resources);
  const MultiFunction &fn = get_vectorized_function(
      base_fn, resources, vnode.rna(), {"use_list__vector"});
  builder.add_function(fn, {0}, {1, 2, 3}, vnode);
}

static void INSERT_list_length(VTreeMFNetworkBuilder &builder,
                               OwnedResources &resources,
                               const VTreeMultiFunctionMappings &mappings,
                               const VNode &vnode)
{
  char *type_name = RNA_string_get_alloc(vnode.rna(), "active_type", nullptr, 0);
  const CPPType &type = *mappings.cpp_type_by_type_name.lookup(type_name);
  MEM_freeN(type_name);

  const MultiFunction &fn = allocate_resource<FN::MF_ListLength>(
      "list length function", resources, type);
  builder.add_function(fn, {0}, {1}, vnode);
}

static void INSERT_get_list_element(VTreeMFNetworkBuilder &builder,
                                    OwnedResources &resources,
                                    const VTreeMultiFunctionMappings &mappings,
                                    const VNode &vnode)
{
  char *type_name = RNA_string_get_alloc(vnode.rna(), "active_type", nullptr, 0);
  const CPPType &type = *mappings.cpp_type_by_type_name.lookup(type_name);
  MEM_freeN(type_name);

  const MultiFunction &fn = allocate_resource<FN::MF_GetListElement>(
      "get list element function", resources, type);
  builder.add_function(fn, {0, 1, 2}, {3}, vnode);
}

static MFBuilderOutputSocket &build_pack_list_node(VTreeMFNetworkBuilder &builder,
                                                   OwnedResources &resources,
                                                   const VNode &vnode,
                                                   const CPPType &base_type,
                                                   const char *prop_name,
                                                   uint start_index)
{
  Vector<bool> input_is_list;
  RNA_BEGIN (vnode.rna(), itemptr, prop_name) {
    int state = RNA_enum_get(&itemptr, "state");
    if (state == 0) {
      /* single value case */
      input_is_list.append(false);
    }
    else if (state == 1) {
      /* list case */
      input_is_list.append(true);
    }
    else {
      BLI_assert(false);
    }
  }
  RNA_END;

  uint input_amount = input_is_list.size();
  uint output_param_index = (input_amount > 0 && input_is_list[0]) ? 0 : input_amount;

  const MultiFunction &fn = allocate_resource<FN::MF_PackList>(
      "pack list function", resources, base_type, input_is_list);
  MFBuilderFunctionNode &node = builder.add_function(
      fn, IndexRange(input_amount).as_array_ref(), {output_param_index});

  for (uint i = 0; i < input_amount; i++) {
    builder.map_sockets(vnode.input(start_index + i), *node.inputs()[i]);
  }

  return *node.outputs()[0];
}

static void INSERT_pack_list(VTreeMFNetworkBuilder &builder,
                             OwnedResources &resources,
                             const VTreeMultiFunctionMappings &mappings,
                             const VNode &vnode)
{
  char *type_name = RNA_string_get_alloc(vnode.rna(), "active_type", nullptr, 0);
  const CPPType &type = *mappings.cpp_type_by_type_name.lookup(type_name);
  MEM_freeN(type_name);

  MFBuilderOutputSocket &packed_list_socket = build_pack_list_node(
      builder, resources, vnode, type, "variadic", 0);
  builder.map_sockets(vnode.output(0), packed_list_socket);
}

static void INSERT_object_location(VTreeMFNetworkBuilder &builder,
                                   OwnedResources &resources,
                                   const VTreeMultiFunctionMappings &UNUSED(mappings),
                                   const VNode &vnode)
{
  const MultiFunction &fn = allocate_resource<FN::MF_ObjectWorldLocation>(
      "object location function", resources);
  builder.add_function(fn, {0}, {1}, vnode);
}

static void INSERT_text_length(VTreeMFNetworkBuilder &builder,
                               OwnedResources &resources,
                               const VTreeMultiFunctionMappings &UNUSED(mappings),
                               const VNode &vnode)
{
  const MultiFunction &fn = allocate_resource<FN::MF_TextLength>("text length function",
                                                                 resources);
  builder.add_function(fn, {0}, {1}, vnode);
}

void add_vtree_node_mapping_info(VTreeMultiFunctionMappings &mappings)
{
  mappings.vnode_inserters.add_new("fn_FloatMathNode", INSERT_float_math);
  mappings.vnode_inserters.add_new("fn_VectorMathNode", INSERT_vector_math);
  mappings.vnode_inserters.add_new("fn_CombineVectorNode", INSERT_combine_vector);
  mappings.vnode_inserters.add_new("fn_SeparateVectorNode", INSERT_separate_vector);
  mappings.vnode_inserters.add_new("fn_ListLengthNode", INSERT_list_length);
  mappings.vnode_inserters.add_new("fn_PackListNode", INSERT_pack_list);
  mappings.vnode_inserters.add_new("fn_GetListElementNode", INSERT_get_list_element);
  mappings.vnode_inserters.add_new("fn_ObjectTransformsNode", INSERT_object_location);
  mappings.vnode_inserters.add_new("fn_TextLengthNode", INSERT_text_length);
}

};  // namespace FN
