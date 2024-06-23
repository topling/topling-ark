
#define popcount __builtin_popcountll

ptrdiff_t bmpopcount(const uint64_t bitmap[4], unsigned char idx)
{
	assert(idx < count);
	int pc = 0;
#define  lastpopcount(i) popcount(bitmap[i] & ~(uint64_t)0 >> (64 - (idx - i * 8)))
	switch (idx / 64)
	{
	case 3:
		pc += popcount(bitmap[0]);
		pc += popcount(bitmap[1]);
		pc += popcount(bitmap[2]);
		pc += lastpopcount(3);
		break;
	case 2:
		pc += popcount(bitmap[0]);
		pc += popcount(bitmap[1]);
		pc += lastpopcount(2);
		break;
	case 1:
		pc += popcount(bitmap[0]);
		pc += lastpopcount(1);
		break;
	case 0:
		pc += lastpopcount(0);
		break;
	}
	return pc;
}

void NodeBase::insertlink(int pc, NodeBase* child)
{
	if (linkcount == linkcapacity) {
		using namespace std;
		int newcapacity = min(int(linkcapacity*3/2), 256);
		NodeBase** newchildren = (NodeBase**)malloc(sizeof(void*)*newcapacity);
		memcpy(newchildren, children, pc);
		newchildren[pc] = child;
		memcpy(newchildren + pc + 1, children + pc, sizeof(void*)*(linkcount - pc));
		children = newchildren;
	}
	else {
		memmove(children + pc + 1, children + pc, sizeof(void*)*(linkcount - pc));
		children[pc] = child;
	}
	linkcount++;
}

static void sample_wordseg(const Trie<int>& dict, unsigned char* text, int length)
{
	for (int i = 0; i < length; ++i) {
		Trie<int>::Node *node = dict.root(), *node2 = node;
		for (int j = i; j < length; ++j) {
			unsigned char ch = text[j];
			node2 = node->getlink(ch);
			if (NULL == node2) {
				node2 = node;
				do {
					int* val = node2->getleaf(text[j]);
					node = node2;
					node2 = node2->parent;
					if (NULL != val) {
						printf("%20.*s\t%6d\n", j - i + 1, text + i, *val);
						break;
					}
					--j;
				} while (NULL != node2 && j >= i);
				break;
			}
			node = node2;
		}
	}
}

