#include "FN_vtree_multi_function_network_builder.h"
#include "FN_vtree_multi_function_network_generation.h"
#include "FN_multi_functions.h"

#include "BLI_math_cxx.h"
#include "BLI_string_map.h"

#include "mappings.h"

namespace FN {

static bool insert_nodes(VTreeMFNetworkBuilder &builder,
                         OwnedResources &resources,
                         const VTreeMultiFunctionMappings &mappings)
{
  const VirtualNodeTree &vtree = builder.vtree();

  for (const VNode *vnode : vtree.nodes()) {
    StringRef idname = vnode->idname();
    const InsertVNodeFunction *inserter = mappings.vnode_inserters.lookup_ptr(idname);

    if (inserter != nullptr) {
      (*inserter)(builder, resources, mappings, *vnode);
      BLI_assert(builder.data_sockets_of_vnode_are_mapped(*vnode));
    }
    else if (builder.has_data_sockets(*vnode)) {
      builder.add_dummy(*vnode);
    }
  }

  return true;
}

static bool insert_links(VTreeMFNetworkBuilder &builder,
                         OwnedResources &resources,
                         const VTreeMultiFunctionMappings &mappings)
{
  for (const VInputSocket *to_vsocket : builder.vtree().all_input_sockets()) {
    ArrayRef<const VOutputSocket *> origins = to_vsocket->linked_sockets();
    if (origins.size() != 1) {
      continue;
    }

    if (!builder.is_data_socket(*to_vsocket)) {
      continue;
    }

    const VOutputSocket *from_vsocket = origins[0];
    if (!builder.is_data_socket(*from_vsocket)) {
      return false;
    }

    MFBuilderOutputSocket &from_socket = builder.lookup_socket(*from_vsocket);
    MFBuilderInputSocket &to_socket = builder.lookup_socket(*to_vsocket);

    if (from_socket.type() == to_socket.type()) {
      builder.add_link(from_socket, to_socket);
    }
    else {
      const InsertImplicitConversionFunction *inserter = mappings.conversion_inserters.lookup_ptr(
          {from_vsocket->idname(), to_vsocket->idname()});
      if (inserter == nullptr) {
        return false;
      }
      auto new_sockets = (*inserter)(builder, resources);
      builder.add_link(from_socket, *new_sockets.first);
      builder.add_link(*new_sockets.second, to_socket);
    }
  }

  return true;
}

static bool insert_unlinked_inputs(VTreeMFNetworkBuilder &builder,
                                   OwnedResources &resources,
                                   const VTreeMultiFunctionMappings &mappings)
{
  Vector<const VInputSocket *> unlinked_data_inputs;
  for (const VInputSocket *vsocket : builder.vtree().all_input_sockets()) {
    if (builder.is_data_socket(*vsocket)) {
      if (!builder.is_input_linked(*vsocket)) {
        unlinked_data_inputs.append(vsocket);
      }
    }
  }

  for (const VInputSocket *vsocket : unlinked_data_inputs) {
    const InsertUnlinkedInputFunction *inserter = mappings.input_inserters.lookup_ptr(
        vsocket->idname());

    if (inserter == nullptr) {
      return false;
    }
    MFBuilderOutputSocket &from_socket = (*inserter)(builder, resources, *vsocket);
    MFBuilderInputSocket &to_socket = builder.lookup_socket(*vsocket);
    builder.add_link(from_socket, to_socket);
  }

  return true;
}

std::unique_ptr<VTreeMFNetwork> generate_vtree_multi_function_network(const VirtualNodeTree &vtree,
                                                                      OwnedResources &resources)
{
  const VTreeMultiFunctionMappings &mappings = get_vtree_multi_function_mappings();

  Vector<MFDataType> type_by_vsocket{vtree.socket_count()};
  for (const VSocket *vsocket : vtree.all_sockets()) {
    MFDataType data_type = mappings.data_type_by_idname.lookup_default(vsocket->idname(),
                                                                       MFDataType::ForNone());
    type_by_vsocket[vsocket->id()] = data_type;
  }

  VTreeMFNetworkBuilder builder(vtree, std::move(type_by_vsocket));
  if (!insert_nodes(builder, resources, mappings)) {
    BLI_assert(false);
  }
  if (!insert_links(builder, resources, mappings)) {
    BLI_assert(false);
  }
  if (!insert_unlinked_inputs(builder, resources, mappings)) {
    BLI_assert(false);
  }

  auto vtree_network = builder.build();
  return vtree_network;
}

}  // namespace FN
