#pragma once

#include <Core/NamesAndAliases.h>
#include <DataTypes/DataTypeString.h>
#include <Storages/ColumnsDescription.h>
#include <Storages/IStorage.h>
#include <Storages/SelectQueryInfo.h>
#include <Storages/System/getQueriedColumnsMaskAndHeader.h>
#include <Processors/Sources/SourceFromSingleChunk.h>
#include <QueryPipeline/Pipe.h>

namespace DB
{

class Context;


/** IStorageSystemOneBlock is base class for system tables whose all columns can be synchronously fetched.
  *
  * Client class need to provide static method static NamesAndTypesList getNamesAndTypes() that will return list of column names and
  * their types. IStorageSystemOneBlock during read will create result columns in same order as result of getNamesAndTypes
  * and pass it with fillData method.
  *
  * Client also must override fillData and fill result columns.
  *
  * If subclass want to support virtual columns, it should override getVirtuals method of IStorage interface.
  * IStorageSystemOneBlock will add virtuals columns at the end of result columns of fillData method.
  */
template <typename Self>
class IStorageSystemOneBlock : public IStorage
{
protected:
    virtual void fillData(MutableColumns & res_columns, ContextPtr context, const SelectQueryInfo & query_info) const = 0;

    virtual bool supportsColumnsMask() const { return false; }

public:
    explicit IStorageSystemOneBlock(const StorageID & table_id_) : IStorage(table_id_)
    {
        StorageInMemoryMetadata storage_metadata;
        storage_metadata.setColumns(Self::getColumnsDescription());
        setInMemoryMetadata(storage_metadata);
    }

    Pipe read(
        const Names & column_names,
        const StorageSnapshotPtr & storage_snapshot,
        SelectQueryInfo & query_info,
        ContextPtr context,
        QueryProcessingStage::Enum /*processed_stage*/,
        size_t /*max_block_size*/,
        size_t /*num_streams*/) override
    {
        storage_snapshot->check(column_names);
        Block sample_block = storage_snapshot->metadata->getSampleBlockWithVirtuals(getVirtuals());

        if (supportsColumnsMask())
        {
            auto [columns_mask, header] = getQueriedColumnsMaskAndHeader(sample_block, column_names);
            query_info.columns_mask = std::move(columns_mask);
            sample_block = std::move(header);
        }

        MutableColumns res_columns = sample_block.cloneEmptyColumns();
        fillData(res_columns, context, query_info);

        UInt64 num_rows = res_columns.at(0)->size();
        Chunk chunk(std::move(res_columns), num_rows);

        return Pipe(std::make_shared<SourceFromSingleChunk>(sample_block, std::move(chunk)));
    }

    bool isSystemStorage() const override { return true; }

    static NamesAndAliases getNamesAndAliases() { return {}; }
};

}
