
namespace terark {

struct NodeBase
{
	uint64_t  bmleaf[4];
	uint64_t  bmlink[4];

	uint16_t  leafcount;
	uint16_t  leafcapacity;
	uint16_t  linkcount;
	uint16_t  linkcapacity;

	NodeBase* parent;
	NodeBase* children;

	NodeBase()
	{
		memset(this, 0, sizeof(*this));
	}

	uint64_t haslink(int idx) const
	{
		assert(idx < 256);
		return bmlink[idx/64] & (uint64_t)1 << idx % 64;
	}
	uint64_t hasleaf(int idx) const
	{
		assert(idx < 256);
		return bmleaf[idx/64] & (uint64_t)1 << idx % 64;
	}

	static void setbm(uint64_t bm[4], int idx)
	{
		assert(idx < 256);
		bm[idx/64] |= (uint64_t)1 << idx % 64;
	}
	static void unsetbm(uint64_t bm[4], int idx)
	{
		assert(idx < 256);
		bm[idx/64] &= ~((uint64_t)1 << idx % 64);
	}

	void insertlink(int pc, NodeBase* child);
};

template<class T>
class Trie
{
// 64bit system is 96 byte
// 32bit system is 84 byte
	class Node : public NodeBase
	{
	friend class Trie;
		T* vals;
	public:
		Node(int nlink, int nleaf)
	   	{
			assert(nlink <= 256);
			assert(nleaf <= 256);
			children = (NodeBase**)malloc(sizeof(void*)*nlink);
		   	vals = (T*)malloc(sizeof(T)*nleaf);
			linkcount = nlink;
			leafcount = nleaf;
	   	}

		~Node()
		{
			clear();
		}

		T* getleaf(int idx) const
	   	{
			if (hasleaf(idx))
				return &vals[bmpopcount(bmleaf, idx)];
			return NULL;
		}

		Node* getlink(int idx) const
		{
			if (haslink(idx))
				return static_cast<Node*>(children[bmpopcount(bmlink, idx)]);
			return NULL;
		}
	private:
		T& insertleaf(int idx, const T& x, bool* existed)
		{
			int pc = bmpopcount(bmleaf, idx);
			if (node->hasleaf(idx)) {
				vals[pc] = x;
				*existed = true;
			}
			else {
				insertleaf_at(pc, x);
				setbm(bmleaf, idx);
				*existed = false;
			}
			return vals[pc];
		}

		void insertleaf_at(int pc, const T& x)
		{
			using namespace std;
			if (leafcount == leafcapacity) {
				int newcapacity = min(int(leafcapacity*3/2), 256);
				T* p = (T*)malloc(sizeof(T)*newcapacity);
				std::uninitialized_copy(vals, vals + pc, p);
				new(p + pc)T(x);
				std::uninitialized_copy(vals + pc, vals + leafcount, p + pc + 1);
				leafcapacity = newcapacity;
			}
			else {
				T* p;
				for (p = vals + leafcount; p > vals + pc; --p)
					*p = std::move(p[-1]);
				new(p)T(x);
			}
			leafcount++;
		}
		bool eraseleaf(int idx)
		{
			if (hasleaf(idx)) {
				int pc = bmpopcount(bmleaf, idx);
				std::copy(vals + pc + 1, vals + leafcount, vals + pc);
				unsetbm(bmleaf, idx);
				leafcount--;
				return true;
			}
			return false;
		}
		void eraselink(int idx)
		{
			assert(haslink(idx));
			int pc = bmpopcount(bmlink, idx);
			memcpy(children + pc, children + pc + 1, sizeof(void*)(linkcount - pc - 1));
			unsetbm(bmlink, idx);
			linkcount--;
		}
		void clear()
		{
			int i;
			for (i = 0; i < linkcount; ++i) {
				children[i]->clear();
				delete static_cast<Node*>(children[i]);
			}
			free(children);
			children = NULL;
			for (i = 0; i < leafcount; ++i) {
				vals[i].~T();
			}
			free(vals);
			vals = NULL;
			memset(bmlink, 0, sizeof(bmlink));
			memset(bmleaf, 0, sizeof(bmleaf));
			leafcount = 0;
			linkcount = 0;
			leafcapacity = 0;
			linkcapacity = 0;
		}
	};

private:
	size_t count;
	Node*  pRoot;

public:
	int size() const { return count; }
	director root() const { return pRoot; }

	T* find(const void* vkey, int len) const
   	{
		if (NULL == pRoot)
			return NULL;
		if (0 == len)
			return NULL;
		const unsigned char* key = (const unsigned char*)vkey;
	   	Node* p = pRoot;
		for (int i = 0; i < len - 1; ++i) {
			int idx = key[i];
			if (p->haslink(idx))
				p = static_cast<Node*>(p->children[bmpopcount(p->bmlink, idx)]);
			else
				return NULL;
		}
		assert(NULL != p);
	}
	T& insert(const void* vkey, int len, const T& val, bool* existed) const
   	{
		const unsigned char* key = (const unsigned char*)vkey;
		if (NULL == pRoot) {
			pRoot = new Node(256, 256);
		}
		Node* p = pRoot;
		for (int i = 0; i < len - 1; ++i) {
			int idx = key[i];
			if (p->haslink(idx))
				p = static_cast<Node*>(p->children[bmpopcount(p->bmlink, idx)]);
			else {
				int pc = bmpopcount(bmlink, idx);
				Node* child = new Node(2, 2);
				p->insertlink(pc, child);
				p->setbm(p->bmlink, idx);
				p = child;
			}
		}
		assert(NULL != p);
		T& ret = p->insertleaf(key[len - 1], val, existed);
		count++;
		return ret;
	}
	bool erase(const void* vkey, int len)
	{
		if (NULL == pRoot)
			return false;
		if (0 == len)
			return false;
		const unsigned char* key = (const unsigned char*)vkey;
	   	Node* p = pRoot;
		for (int i = 0; i < len - 1; ++i) {
			int idx = key[i];
			if (p->haslink(idx))
				p = static_cast<Node*>(p->children[bmpopcount(p->bmlink, idx)]);
			else
				return false;
		}
		assert(NULL != p);
		bool bRet = eraseleaf(key[len-1]);
		for (int i = len - 1; i > 1; --i) {
			int idx = key[i];
			Node* pp = p->parent;
			if (0 == p->leafcount && 0 == p->linkcount) {
				pp->eraselink(key[i-1]);
			}
			p = pp;
		}
		count--;
		return bRet;
	}

	void clear()
	{
		pRoot->clear();
		count = 0;
		delete pRoot;
		pRoot = NULL;
	}
};

} // namespace terark

