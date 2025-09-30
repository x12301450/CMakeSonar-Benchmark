#include "./blob_manager_impl.h"
#include <set>
#include "megbrain/utils/arith_helper.h"

namespace mgb {
namespace imperative {

BlobManagerImpl::BlobData::BlobData(OwnedBlob* in_blob) {
    blob = in_blob;
    DeviceTensorStorage d_storage;
    d_storage.reset(blob->m_comp_node, blob->m_size, blob->m_storage);
    h_storage = HostTensorStorage(blob->m_comp_node);
    h_storage.ensure_size(blob->m_size);
    h_storage.copy_from(const_cast<DeviceTensorStorage&>(d_storage), blob->m_size);
}

void BlobManagerImpl::register_blob(OwnedBlob* blob) {
    // add blob into the comp2blobs map
    MGB_LOCK_GUARD(m_mtx);
    mgb_assert(m_comp2blobs_map[blob->m_comp_node].insert(blob));
}

void BlobManagerImpl::unregister_blob(OwnedBlob* blob) {
    // erase blob into the comp2blobs map
    MGB_LOCK_GUARD(m_mtx);
    mgb_assert(1 == m_comp2blobs_map[blob->m_comp_node].erase(blob));
}

void BlobManagerImpl::alloc_with_defrag(OwnedBlob* blob, size_t size) {
    if (m_custom_allocator) {
        blob->m_storage = m_custom_allocator(blob->m_comp_node, size);
        return;
    }
    // try alloc
    // if fail, try defrag, alloc again
    if (!try_alloc_direct(blob, size)) {
        mgb_log_warn("memory allocation failed for blob; try defragmenting");
        defrag(blob->m_comp_node);
        alloc_direct(blob, size);
    }
}

void BlobManagerImpl::alloc_direct(OwnedBlob* blob, size_t size) {
    mgb_assert(blob->m_comp_node.valid());
    DeviceTensorStorage storage(blob->m_comp_node);
    storage.ensure_size(size);
    blob->m_storage = storage.raw_storage();
}

void BlobManagerImpl::set_allocator(allocator_t allocator) {
    m_custom_allocator = allocator;
}

void BlobManagerImpl::defrag(const CompNode& cn) {
    auto& blobs_set_ptr = ([&]() -> auto& {
        MGB_LOCK_GUARD(m_mtx);
        return m_comp2blobs_map[cn];
    })();
    MGB_LOCK_GUARD(blobs_set_ptr.mtx);
    std::vector<BlobData> blob_data_arrary;
    std::set<Blob::RawStorage> storage_set;

    auto alignment = cn.get_mem_addr_alignment();
    size_t tot_sz = 0;

    // copy to HostTensorStorage, and release
    for (auto i : blobs_set_ptr.blobs_set) {
        // skip if blob do not have m_storage
        if (!i->m_storage)
            continue;

        // skip if ues_count() > 1
        if (i->m_storage.use_count() > 1)
            continue;

        // two blobs can't share same storage
        mgb_assert(storage_set.insert(i->m_storage).second);

        tot_sz += get_aligned_power2(i->m_size, alignment);
        BlobData blob_data(i);
        blob_data_arrary.push_back(blob_data);
        i->m_storage.reset();
    }
    // clear all, make sure m_storage will be release
    storage_set.clear();

    // skip if no blob to defrag
    if (!blob_data_arrary.size())
        return;

    // wait all other comp nodes to avoid moved var being read; note that
    // ExecEnv has been paused, so no new task would not be dispatched
    CompNode::sync_all();
    CompNode::try_coalesce_all_free_memory();

    // try free all
    MGB_TRY { cn.free_device(cn.alloc_device(tot_sz)); }
    MGB_CATCH(MemAllocError&, {})

    // sort blobs by created time, may be helpful for reduce memory fragment
    std::sort(
            blob_data_arrary.begin(), blob_data_arrary.end(),
            [](auto& lhs, auto& rhs) { return lhs.blob->m_id < rhs.blob->m_id; });

    // allocate for each storage
    for (auto i : blob_data_arrary) {
        DeviceTensorStorage d_storage = DeviceTensorStorage(cn);
        d_storage.ensure_size(i.blob->m_size);
        d_storage.copy_from(i.h_storage, i.blob->m_size);
        i.blob->m_storage = d_storage.raw_storage();
    }

    // wait copy finish before destructing host values
    cn.sync();
}

struct BlobManagerStub : BlobManager {
    void alloc_direct(OwnedBlob* blob, size_t size) {
        mgb_assert(0, "prohibited after global variable destruction");
    };
    void alloc_with_defrag(OwnedBlob* blob, size_t size) {
        mgb_assert(0, "prohibited after global variable destruction");
    };
    void register_blob(OwnedBlob* blob) {
        mgb_assert(0, "prohibited after global variable destruction");
    };
    void unregister_blob(OwnedBlob* blob){};
    void defrag(const CompNode& cn) {
        mgb_assert(0, "prohibited after global variable destruction");
    };
    void set_allocator(allocator_t allocator) {
        mgb_assert(0, "prohibited after global variable destruction");
    };
};

BlobManager* BlobManager::inst() {
    static std::aligned_union_t<0, BlobManagerImpl, BlobManagerStub> storage;

    struct Keeper {
        Keeper() { new (&storage) BlobManagerImpl(); }
        ~Keeper() {
            reinterpret_cast<BlobManager*>(&storage)->~BlobManager();
            new (&storage) BlobManagerStub();
        }
    };
    static Keeper _;

    return reinterpret_cast<BlobManager*>(&storage);
}

}  // namespace imperative
}  // namespace mgb
