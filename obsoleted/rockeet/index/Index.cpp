#include "Index.hpp"
#include <terark/io/MemStream.hpp>
#include <terark/io/DataIO.hpp>
#include <terark/io/DataIO_SmartPtr.hpp>
#include <terark/set_op.hpp>
#include <terark/io/FileStream.hpp>
#include <terark/num_to_str.hpp>

namespace terark { namespace rockeet {

 	template<class DataIO>
 	void DataIO_loadObject(DataIO& dio, DeltaInvertIndex& x)
	{
		x.load(dio, 1);
	}
 	template<class DataIO>
 	void DataIO_saveObject(DataIO& dio, const DeltaInvertIndex& x)
	{
		x.save(dio, 1);
	}

	Index* Index::fromFile(const std::string& fname)
	{
		FileStream file(fname.c_str(), "rb");
		file.disbuf();
		PortableDataInput<InputBuffer> ibuf; ibuf.attach(&file);
		uint32_t version;
		var_size_t fieldcount;

		Index* index = new Index();
		ibuf >> version;
		ibuf >> fieldcount;
		for (uint32_t i = 0; i < fieldcount.t; ++i)
		{
			std::string  fieldname;
			int fieldtype;
			ibuf >> fieldname;
			ibuf >> fieldtype;
			index->fields[fieldname] = DeltaInvertIndex::builtInFixedKeyIndex(field_type_t(fieldtype));
		}
		index->readIndexData(ibuf, version);

		return index;
	}

	void Index::readIndexData(PortableDataInput<InputBuffer>& ibuf, unsigned version)
	{
		for (fieldset_t::iterator iter = fields.begin(); fields.end() != iter; ++iter)
		{
			iter->second->load(ibuf, version);
		}
	}

	void Index::savetoFile(const std::string& fname) const
	{
		FileStream file(fname.c_str(), "wb");
		file.disbuf();
		PortableDataOutput<OutputBuffer> obuf; obuf.attach(&file);
		uint32_t version = 1;
		obuf << version;
		obuf << var_size_t(fields.size());
		for (fieldset_t::const_iterator iter = fields.begin(); fields.end() != iter; ++iter)
		{
			obuf << iter->first;
			obuf << int(iter->second->getKeyType());
		}
		for (fieldset_t::const_iterator iter = fields.begin(); fields.end() != iter; ++iter)
		{
			iter->second->save(obuf, version);
		}
	}

	namespace { // anonymous namespace
		struct PostingList
		{
			PortableDataOutput<AutoGrownMemIO> list;
			uint64_t maxDocID;
			uint32_t nDoc;
			uint32_t nDeleted;

	DATA_IO_LOAD_SAVE(PostingList, &maxDocID &nDoc &nDeleted &(AutoGrownMemIO&)list)

			//--------------------------------------------
			// followed by key data, including keylen
			//  char key[0];
			//--------------------------------------------

			PostingList(uint64_t maxDocID = 0)
				: maxDocID(maxDocID)
				, nDoc(0)
				, nDeleted(0)
			{}

			static void destroy(const struct trb_vtab* vtab,
						 const struct trb_tree* tree,
						 void* data
						 )
			{
				PostingList* pl = (PostingList*)data;
				pl->~PostingList();
			}
		};
	} // anonymous namespace

	DeltaInvertIndex::DeltaInvertIndex()
    {
        memset(&m_vtab, 0, sizeof(trb_vtab));
        memset(&m_tree, 0, sizeof(trb_tree));
        m_maxDocID = 0;
		m_pfGetKeySize = NULL;
        m_vtab.key_offset = sizeof(PostingList);
		m_vtab.app_data = this;
		m_vtab.pf_data_destroy = &PostingList::destroy;
    }

	DeltaInvertIndex::~DeltaInvertIndex()
    {
		trb_destroy(&m_vtab, &m_tree);
    }

    DeltaInvertIndex* DeltaInvertIndex::builtInFixedKeyIndex(field_type_t type)
    {
        DeltaInvertIndex* index = new DeltaInvertIndex();
		index->m_vtab.data_size = field_type_size(type) + sizeof(PostingList);
        trb_vtab_init(&index->m_vtab, type);
		return index;
	}

    DeltaInvertIndex* DeltaInvertIndex::fixedKeyIndex(int keySize, trb_func_compare compare)
	{
        DeltaInvertIndex* index = new DeltaInvertIndex();
		index->m_vtab.data_size = keySize + sizeof(PostingList);
		index->m_vtab.key_size = keySize;
		index->m_vtab.pf_compare = compare;
        trb_vtab_init(&index->m_vtab, tev_app);
		return index;
	}

	// 用户提供的是 pfGetKeySize, 要再加上 sizeof(PostingList)
	size_t DeltaInvertIndex::trbGetDataSize(const struct trb_vtab* vtab, const void* data)
	{
		DeltaInvertIndex* self = (DeltaInvertIndex*)vtab->app_data;
		return self->m_pfGetKeySize(vtab, data) + sizeof(PostingList);
	}

    DeltaInvertIndex* DeltaInvertIndex::varKeyIndex(trb_func_compare compare, trb_func_data_get_size pfGetKeySize)
	{
        DeltaInvertIndex* index = new DeltaInvertIndex();
		index->m_vtab.pf_compare = compare;
		index->m_vtab.pf_data_get_size = &DeltaInvertIndex::trbGetDataSize;
        trb_vtab_init(&index->m_vtab, tev_app);
		return index;
	}

    void DeltaInvertIndex::append(uint64_t docID, const TermInfo& ti)
    {
        // keylen == 0 implies caller ignored it
        // if keylen != 0, caller must ensure keylen == m_vtab.key_size
        assert(ti.cKey == 0 || ti.cKey == m_vtab.key_size);

		size_t nodeSize = m_vtab.data_offset + sizeof(PostingList) + ti.cKey ? ti.cKey : m_vtab.key_size;
		trb_node* node1 = (trb_node*)m_vtab.alloc->salloc(m_vtab.alloc, nodeSize);
        memcpy(node1+1, ti.pKey, ti.cKey);
        trb_node* node2 = trb_probe_node(&m_vtab, &m_tree, node1);
        PostingList* pl = (PostingList*)(node2+1);
        if (node2 == node1) { // not existed, inserted node1
            // initialize
            new(pl)PostingList(docID);
            pl->list << var_uint64_t(docID);
        } else {
            m_vtab.alloc->sfree(m_vtab.alloc, node1, nodeSize);
            pl->list << var_uint64_t(docID-pl->maxDocID); // diff encoding
            pl->maxDocID = docID;
        }
        pl->list.ensureWrite(ti.pVal, ti.cVal);
    }

    /**
     * keylen should determined by key's binary data
     */
    IPostingRange* DeltaInvertIndex::fuzzyFind(const void* key)
	{
        trb_node* node = m_vtab.pf_find(&m_vtab, &m_tree, key);
        if (node) {
            PostingList* pl = (PostingList*)(node+1);
            return new MemPostingRange(pl->list.begin(), pl->list.tell(), pl->nDoc);
        }
        return NULL;
    }

    QueryResultPtr DeltaInvertIndex::find(const void* key)
	{
        trb_node* node = m_vtab.pf_find(&m_vtab, &m_tree, key);
        if (node) {
            PostingList* pl = (PostingList*)(node+1);
            TermQueryResult* result = new TermQueryResult(pl->list.begin(), pl->list.end(), pl->nDoc);
			result->initCursor();
			return result;
        }
        return NULL;
    }

	void DeltaInvertIndex::load(PortableDataInput<InputBuffer>& input, unsigned version)
	{
		var_uint64_t count;
		input >> count;
		if (m_pfGetKeySize)
		{
			for (uint64_t i = 0; i < count.t; ++i)
			{
				var_size_t keySize;
				input >> keySize;
				size_t nodeSize = sizeof(trb_node) + sizeof(PostingList) + keySize;
				trb_node* node1 = (trb_node*)m_vtab.alloc->salloc(m_vtab.alloc, nodeSize);
				trb_node* node2 = trb_probe_node(&m_vtab, &m_tree, node1);
				PostingList* pl = (PostingList*)(node1 + 1);
				input.ensureRead(pl + 1, keySize);
				new(pl)PostingList();
				input >> *pl;
			}
		}
		else
		{
			for (uint64_t i = 0; i < count.t; ++i)
			{
				size_t nodeSize = sizeof(trb_node) + sizeof(PostingList) + m_vtab.key_size;
				trb_node* node1 = (trb_node*)m_vtab.alloc->salloc(m_vtab.alloc, nodeSize);
				trb_node* node2 = trb_probe_node(&m_vtab, &m_tree, node1);
				PostingList* pl = (PostingList*)(node1 + 1);
				input.ensureRead(pl + 1, m_vtab.key_size);
				new(pl)PostingList();
				input >> *pl;
			}
		}
	}

	void DeltaInvertIndex::save(PortableDataOutput<OutputBuffer>& out, unsigned version) const
	{
		out << var_uint64_t(m_tree.trb_count);

		if (m_pfGetKeySize)
		{
			for (trb_node* node = trb_iter_first(m_vtab.node_offset, &m_tree); NULL != node; node = trb_iter_next(m_vtab.node_offset, node))
			{
				size_t keySize = m_pfGetKeySize(&m_vtab, (PostingList*)(node + 1) + 1);
				out << var_size_t(keySize);
				out.ensureWrite((PostingList*)(node+1)+1, keySize);
				out << *(PostingList*)(node+1);
			}
		}
		else // don't store keySize
		{
			for (trb_node* node = trb_iter_first(m_vtab.node_offset, &m_tree); NULL != node; node = trb_iter_next(m_vtab.node_offset, node))
			{
				out.ensureWrite((PostingList*)(node+1)+1, m_vtab.key_size);
				out << *(PostingList*)(node+1);
			}
		}
	}

//////////////////////////////////////////////////////////////////////////

	Index::Index()
	{
		maxDocID = 0;
		strictIndexType = true;
	}

	uint64_t Index::append(const DocTermVec& doc)
	{
		for (DocTermVec::const_iterator i = doc.begin(); i != doc.end(); ++i)
		{
			fieldset_t::iterator fieldIndexIter = fields.find(i->first);
			if (fieldIndexIter == fields.end())
			{
				if (strictIndexType)
				{
					string_appender<> oss;
					oss << "field=\"" << i->first << "\" is not existed in InvertIndex!";
					throw std::runtime_error(oss.str());
				}
				else // create the FieldIndex
				{
				// ignore ?
				//	fields[i->first] = DeltaInvertIndex::;
				}
			}
			else // found
			{
				for (std::vector<ITerm*>::const_iterator j = i->second.begin(); j != i->second.end(); ++j)
				{
					ITerm* t = *j;
					TermInfo ti;
					t->getInfo(&ti);
					assert(ti.keyType == fieldIndexIter->second->getKeyType());
					fieldIndexIter->second->append(maxDocID, ti);
				}
			}
		}
		return maxDocID++;
	}

	void Index::addField(const std::string& fieldname, DeltaInvertIndexPtr fieldindex)
	{
		fields[fieldname] = fieldindex;
	}

	void Index::save(PortableDataOutput<OutputBuffer>& output, unsigned version)
	{
		output & maxDocID;
		output & fields;
	}

	void Index::load(PortableDataInput<InputBuffer>& input, unsigned version)
	{
		input & maxDocID;
		var_size_t fieldcount;
		input >> fieldcount;
		for (int i = 0; i != fieldcount.t; ++i)
		{
			std::string fieldname;
			input & fieldname;
			DeltaInvertIndexPtr fieldindex = fields[fieldname];
			if (fieldindex) {
				input >> *fieldindex;
			} else {
				string_appender<> oss;
				oss << "Index::load: unknown field: " << fieldname;
				throw std::runtime_error(oss.str());
			}
		}
	}

	std::vector<uint64_t> Index::search(const std::vector<std::pair<std::string, std::string> >& qf, const char* op)
	{
		using namespace std;
		std::vector<std::vector<uint64_t> > vv(qf.size());
		for (ptrdiff_t i = 0; i != qf.size(); ++i)
		{
			std::string fieldname = qf[i].first;
			std::string field_key = qf[i].second;
			fieldset_t::const_iterator f = fields.find(fieldname);
			if (fields.end() != f)
			{
				std::vector<uint64_t> v;
				DeltaInvertIndexPtr index = f->second;
				IPostingRange* ipr = NULL;
				switch (index->getKeyType())
				{
				default:
					throw std::runtime_error("not supported keyType");
					break;
				case tev_int:
					{
						int key = atoi(field_key.c_str());
						ipr = index->fuzzyFind(&key);
					}
					break;
				case tev_cstr:
					ipr = index->fuzzyFind(field_key.c_str());
					break;
				}
				if (ipr && !ipr->empty())
				{
					v.resize(ipr->size());
					while (!ipr->empty())
					{
						uint64_t docID = ipr->deref().docID;
						v.push_back(docID);
					}
					vv[i].swap(v);
				}
			}
		}
		std::vector<uint64_t> result;
		if (vv.size() == 1)
		{
			result.swap(vv[0]);
		}
		else if (vv.size() == 2)
		{
			if (strcmp(op, "or") == 0)
				std::set_union(vv[0].begin(), vv[0].end(), vv[1].begin(), vv[1].end(), std::back_inserter(result));
			else if (strcmp(op, "and") == 0)
				std::set_intersection(vv[0].begin(), vv[0].end(), vv[1].begin(), vv[1].end(), std::back_inserter(result));
			else {
				throw runtime_error("not supported op");
			}
		}
		else
		{
			typedef std::vector<TT_PAIR(std::vector<uint64_t>::iterator) > iter_vec_t;
			iter_vec_t iv;
			for (ptrdiff_t i = 0; i != vv.size(); ++i)
			{
			//	if (!vv[i].empty())
			//		iv.push_back()
				iv.push_back(make_pair(vv[i].begin(), vv[i].end()));
			}
			typedef multi_way::HeapMultiWay<iter_vec_t::iterator> mw_t;
			mw_t mw(iv.begin(), iv.end());
			if (strcmp(op, "or") == 0)
				mw.union_set(back_inserter(result));
			else if (strcmp(op, "and") == 0)
				mw.intersection(back_inserter(result));
			else {
				throw runtime_error("not supported op");
			}
		}
		return result;
	}

	QueryResultPtr Index::searchTermImpl(const std::string& fieldname, const void* k, field_type_t t) const
	{
		fieldset_t::const_iterator f = fields.find(fieldname);
		if (fields.end() == f)
		{
			return new TempQueryResult();
		}
		DeltaInvertIndex& index = *f->second;
		assert(index.getKeyType() == t);
		if (index.getKeyType() != t) {
			throw std::logic_error("field type mismatch");
		}
		return index.find(k);
	}


} } // namespace terark::rockeet
